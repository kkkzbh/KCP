package org.cp.lang.clion

import com.intellij.execution.RunManager
import com.intellij.openapi.fileEditor.FileDocumentManager
import com.intellij.openapi.roots.ProjectRootManager
import com.intellij.openapi.vfs.LocalFileSystem
import com.intellij.openapi.vfs.VfsUtilCore
import com.intellij.openapi.vfs.VirtualFile
import com.intellij.psi.PsiFile
import com.intellij.psi.search.FileTypeIndex
import com.intellij.psi.search.GlobalSearchScope
import java.nio.file.Path
import java.util.concurrent.ConcurrentHashMap

data class CpSnapshotSource(
    val path: String,
    val text: String,
)

data class CpSnapshotContext(
    val activePath: String,
    val activeText: String,
    val targetPath: String,
    val importRoots: List<String>,
    val moduleSearchRoots: List<String>,
    val projectFiles: List<CpSnapshotSource>,
) {
    fun resolve(): CpInspectionRequest =
        CpProjectSnapshotCollector.buildRequest(
            activePath = activePath,
            activeText = activeText,
            targetPath = targetPath,
            importRoots = importRoots,
            moduleSearchRoots = moduleSearchRoots,
            projectFiles = projectFiles,
        ).also { request ->
            CpProjectSnapshotCollector.traceResolved(this, request)
        }
}

object CpProjectSnapshotCollector {
    fun collect(file: PsiFile, activeText: String): CpInspectionRequest? =
        prepare(file, activeText)?.resolve()

    internal fun prepare(file: PsiFile, activeText: String): CpSnapshotContext? {
        if (file.language != CpLanguage) {
            return null
        }

        val activePath = file.virtualFile?.path ?: file.name
        val targetPath = selectedTargetPath(file) ?: activePath
        val importRoots = importRoots(file, targetPath)
        val moduleSearchRoots = moduleSearchRoots(file, activePath, targetPath)
        return CpSnapshotContext(
            activePath = activePath,
            activeText = activeText,
            targetPath = targetPath,
            importRoots = importRoots,
            moduleSearchRoots = moduleSearchRoots,
            projectFiles = unsavedCpFiles() + nonLocalProjectCpFiles(file),
        )
    }

    internal fun traceResolved(context: CpSnapshotContext, request: CpInspectionRequest) {
        CpDiagnosticsTrace.info("snapshot:${request.activeFile}:${request.files.map { it.path }}") {
            "cp snapshot active=${request.activeFile} target=${context.targetPath} " +
                "importRoots=${context.importRoots} moduleSearchRoots=${context.moduleSearchRoots} " +
                "files=${request.files.map { it.path }}"
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

        val resolveRequest = CpInspectionResolveRequest(
            activeFile = active,
            targetFile = target,
            importRoots = roots,
            searchRoots = searchRoots,
            files = knownFiles.values.toList(),
        )
        return CpResolvedRequestCache.resolve(
            request = resolveRequest,
            knownFiles = knownFiles,
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
        return FileTypeIndex.getFiles(CpFileType.INSTANCE, GlobalSearchScope.projectScope(file.project))
            .asSequence()
            .filter { it.isValid && !it.isDirectory && it.fileSystem != LocalFileSystem.getInstance() }
            .map { virtualFile ->
                CpSnapshotSource(
                    path = virtualFile.path,
                    text = loadVirtualFileText(virtualFile),
                )
            }
            .distinctBy { cpNormalizePath(it.path) }
            .toList()
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
        return cpSourceImportRoots(
            source = source,
            projectBasePath = file.project.basePath,
            stdlibRoot = compiler?.let { CpRunPaths.resolveStdlibRoot(file.project, source, it) },
        )
            .map { it.toString() }
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

internal object CpResolvedRequestCache {
    private val entries = ConcurrentHashMap<CpResolvedRequestCacheKey, List<CpInspectionFile>>()

    fun resolve(request: CpInspectionResolveRequest, knownFiles: Map<String, CpInspectionFile> = emptyMap()): CpInspectionRequest? {
        val key = request.cacheKey()
        entries[key]?.let { files ->
            CpDiagnosticsTrace.info("snapshot-resolve-cache-hit:${request.activeFile}:${request.targetFile}") {
                "cp snapshot resolve cache hit active=${request.activeFile} target=${request.targetFile}"
            }
            return CpInspectionRequest(
                activeFile = request.activeFile,
                files = files.withKnownTexts(knownFiles),
            )
        }

        val resolved = CpHelperRunner.resolveInspectionRequest(request) ?: return null
        if (entries.size > maxEntries) {
            entries.clear()
        }
        entries[key] = resolved.files
        return resolved
    }

    private fun CpInspectionResolveRequest.cacheKey(): CpResolvedRequestCacheKey =
        CpResolvedRequestCacheKey(
            activeFile = activeFile,
            targetFile = targetFile,
            entryFiles = entryFiles,
            importRoots = importRoots,
            searchRoots = searchRoots,
            followStdlibImports = followStdlibImports,
            files = files
                .map { CpResolvedRequestFileKey(it.path, importSignature(it.text)) }
                .sortedBy { it.path },
        )

    private fun List<CpInspectionFile>.withKnownTexts(knownFiles: Map<String, CpInspectionFile>): List<CpInspectionFile> =
        map { file -> knownFiles[file.path] ?: file }

    private fun importSignature(text: String): String =
        text
            .lineSequence()
            .map(String::trim)
            .filter { line ->
                line.startsWith("import ") ||
                    line.startsWith("module ") ||
                    line.startsWith("export import ") ||
                    line.startsWith("export module ")
            }
            .joinToString("\n")

    private const val maxEntries = 256
}

private data class CpResolvedRequestCacheKey(
    val activeFile: String,
    val targetFile: String,
    val entryFiles: List<String>,
    val importRoots: List<String>,
    val searchRoots: List<String>,
    val followStdlibImports: Boolean,
    val files: List<CpResolvedRequestFileKey>,
)

private data class CpResolvedRequestFileKey(
    val path: String,
    val importSignature: String,
)
