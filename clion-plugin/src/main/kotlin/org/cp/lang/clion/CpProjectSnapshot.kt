package org.cp.lang.clion

import com.intellij.execution.RunManager
import com.intellij.openapi.fileEditor.FileDocumentManager
import com.intellij.openapi.roots.ProjectRootManager
import com.intellij.openapi.vfs.LocalFileSystem
import com.intellij.openapi.vfs.VfsUtilCore
import com.intellij.openapi.vfs.VirtualFile
import com.intellij.psi.PsiFile
import java.nio.file.Path

data class CpSnapshotSource(
    val path: String,
    val text: String,
)

object CpProjectSnapshotCollector {
    fun collect(file: PsiFile, activeText: String): CpInspectionRequest? {
        if (file.language != CpLanguage) {
            return null
        }

        val activePath = file.virtualFile?.path ?: file.name
        val targetPath = selectedTargetPath(file) ?: activePath
        val importRoots = importRoots(file, targetPath)
        val moduleSearchRoots = moduleSearchRoots(file, activePath, targetPath)
        return buildRequest(
            activePath = activePath,
            activeText = activeText,
            targetPath = targetPath,
            importRoots = importRoots,
            moduleSearchRoots = moduleSearchRoots,
            projectFiles = unsavedCpFiles() + nonLocalProjectCpFiles(file),
        ).also { request ->
            CpDiagnosticsTrace.info("snapshot:${request.activeFile}:${request.files.map { it.path }}") {
                "cp snapshot active=${request.activeFile} target=$targetPath " +
                    "importRoots=$importRoots moduleSearchRoots=$moduleSearchRoots " +
                    "files=${request.files.map { it.path }}"
            }
        }
    }

    fun buildRequest(
        activePath: String,
        activeText: String,
        projectFiles: Iterable<CpSnapshotSource>,
        targetPath: String = activePath,
        importRoots: Iterable<String> = emptyList(),
        moduleSearchRoots: Iterable<String> = emptyList(),
    ): CpInspectionRequest {
        val active = cpNormalizePath(activePath)
        val target = cpNormalizePath(targetPath)
        val roots = importRoots.map(::cpNormalizePath)
        val searchRoots = moduleSearchRoots
            .plus(listOfNotNull(Path.of(active).parent?.toString(), Path.of(target).parent?.toString()))
            .map(::cpNormalizePath)
            .distinct()
            .toList()
        val knownFiles = linkedMapOf<String, CpInspectionFile>()
        for (source in projectFiles) {
            val path = cpNormalizePath(source.path)
            knownFiles[path] = CpInspectionFile(path = path, text = source.text)
        }
        knownFiles[active] = CpInspectionFile(path = active, text = activeText)

        return CpHelperRunner.resolveInspectionRequest(
            CpInspectionResolveRequest(
                activeFile = active,
                targetFile = target,
                importRoots = roots,
                searchRoots = searchRoots,
                files = knownFiles.values.toList(),
            ),
        ) ?: CpInspectionRequest(
            activeFile = active,
            files = listOf(CpInspectionFile(path = active, text = activeText)),
        )
    }

    private fun unsavedCpFiles(): List<CpSnapshotSource> {
        val fileDocumentManager = FileDocumentManager.getInstance()
        return fileDocumentManager.unsavedDocuments
            .asSequence()
            .mapNotNull { document ->
                val virtualFile = fileDocumentManager.getFile(document)
                    ?.takeIf { it.isValid && !it.isDirectory && it.extension == CpFileType.INSTANCE.defaultExtension }
                    ?: return@mapNotNull null
                CpSnapshotSource(
                    path = virtualFile.path,
                    text = document.text,
                )
            }
            .distinctBy { cpNormalizePath(it.path) }
            .toList()
    }

    private fun nonLocalProjectCpFiles(file: PsiFile): List<CpSnapshotSource> {
        val files = mutableListOf<CpSnapshotSource>()
        val roots = ProjectRootManager.getInstance(file.project).contentRoots
        for (root in roots) {
            if (root.fileSystem == LocalFileSystem.getInstance()) {
                continue
            }
            VfsUtilCore.iterateChildrenRecursively(root, null) { virtualFile ->
                if (!virtualFile.isDirectory && virtualFile.extension == CpFileType.INSTANCE.defaultExtension) {
                    files += CpSnapshotSource(
                        path = virtualFile.path,
                        text = loadVirtualFileText(virtualFile),
                    )
                }
                true
            }
        }
        return files
    }

    private fun loadVirtualFileText(file: VirtualFile): String =
        FileDocumentManager.getInstance().getDocument(file)?.text
            ?: VfsUtilCore.loadText(file)

    private fun selectedTargetPath(file: PsiFile): String? {
        val configuration = RunManager.getInstance(file.project).selectedConfiguration?.configuration as? CpRunConfiguration
            ?: return null
        return configuration.mainFile.takeUnless { it.isBlank() }
    }

    private fun importRoots(file: PsiFile, targetPath: String): List<String> {
        val source = Path.of(targetPath)
        val configuration = RunManager.getInstance(file.project).selectedConfiguration?.configuration as? CpRunConfiguration
        val compiler = configuration?.let { CpRunPaths.resolveCompiler(file.project, it.compilerPath, source) }
        return source.parent.parentsFromSelf()
            .map { it.toString() }
            .plus(
                listOfNotNull(
                    file.project.basePath,
                    compiler?.let { CpRunPaths.resolveStdlibRoot(file.project, source, it)?.toString() },
                ).asSequence(),
            )
            .distinct()
            .toList()
    }

    private fun moduleSearchRoots(file: PsiFile, activePath: String, targetPath: String): List<String> {
        val projectRoots = ProjectRootManager.getInstance(file.project).contentRoots
            .asSequence()
            .filter { it.fileSystem == LocalFileSystem.getInstance() }
            .map { it.path }
        return sequenceOf(
            Path.of(activePath).parent?.toString(),
            Path.of(targetPath).parent?.toString(),
        )
            .filterNotNull()
            .plus(file.project.basePath?.let { sequenceOf(it) } ?: emptySequence())
            .plus(projectRoots)
            .map(::cpNormalizePath)
            .distinct()
            .toList()
    }
}
