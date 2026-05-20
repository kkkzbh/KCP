package org.cp.lang.clion

import com.intellij.execution.RunManager
import com.intellij.openapi.diagnostic.Logger
import com.intellij.openapi.fileEditor.FileDocumentManager
import com.intellij.openapi.vfs.VfsUtilCore
import com.intellij.openapi.vfs.VirtualFile
import com.intellij.psi.PsiFile
import com.intellij.psi.search.FileTypeIndex
import com.intellij.psi.search.GlobalSearchScope
import java.nio.file.Files
import java.nio.file.Path

data class CpSnapshotSource(
    val path: String,
    val text: String,
)

object CpProjectSnapshotCollector {
    private val log: Logger = Logger.getInstance(CpProjectSnapshotCollector::class.java)
    private val importRegex = Regex("""(?m)^\s*(?:export\s+)?import\s+([A-Za-z_][A-Za-z0-9_]*(?:\.[A-Za-z_][A-Za-z0-9_]*)*)\s*;""")

    fun collect(file: PsiFile, activeText: String): CpInspectionRequest? {
        if (file.language != CpLanguage) {
            return null
        }

        val activePath = file.virtualFile?.path ?: file.name
        val targetPath = selectedTargetPath(file) ?: activePath
        val importRoots = importRoots(file, targetPath)
        val projectFiles = runCatching {
            FileTypeIndex.getFiles(CpFileType.INSTANCE, GlobalSearchScope.projectScope(file.project))
                .asSequence()
                .filter { it.isValid && !it.isDirectory }
                .mapNotNull { virtualFile ->
                    runCatching {
                        CpSnapshotSource(
                            path = virtualFile.path,
                            text = loadEditorText(virtualFile),
                        )
                    }.getOrElse { exception ->
                        log.warn("failed to read cp project file ${virtualFile.path}", exception)
                        null
                    }
                }
                .toList()
        }.getOrElse { exception ->
            log.warn("failed to collect cp project files", exception)
            emptyList()
        }

        return buildRequest(
            activePath = activePath,
            activeText = activeText,
            targetPath = targetPath,
            importRoots = importRoots,
            projectFiles = projectFiles,
        )
    }

    fun buildRequest(
        activePath: String,
        activeText: String,
        projectFiles: Iterable<CpSnapshotSource>,
        targetPath: String = activePath,
        importRoots: Iterable<String> = emptyList(),
    ): CpInspectionRequest {
        val sourceTexts = linkedMapOf<String, String>()
        for (projectFile in projectFiles) {
            sourceTexts[normalizedPath(projectFile.path)] = projectFile.text
        }
        sourceTexts[normalizedPath(activePath)] = activeText

        val files = linkedMapOf<String, String>()
        val loaded = mutableSetOf<String>()
        val pending = mutableListOf(normalizedPath(targetPath))
        val roots = importRoots.map { normalizedPath(it) }

        var index = 0
        while (index < pending.size) {
            val path = pending[index]
            index += 1
            if (!loaded.add(path)) {
                continue
            }
            val text = sourceTexts[path] ?: readText(path) ?: continue
            files[path] = text

            val base = Path.of(path).parent
            for (moduleName in importedModules(text)) {
                val resolved = resolveImportPath(base, moduleName, roots, sourceTexts) ?: continue
                if (resolved !in loaded) {
                    pending.add(resolved)
                }
            }
        }

        val active = normalizedPath(activePath)
        if (active !in files) {
            return CpInspectionRequest(
                activeFile = active,
                files = listOf(CpInspectionFile(path = active, text = activeText)),
            )
        }

        return CpInspectionRequest(
            activeFile = active,
            files = files.map { (path, text) ->
                CpInspectionFile(path = path, text = text)
            },
        )
    }

    private fun loadEditorText(virtualFile: VirtualFile): String =
        FileDocumentManager.getInstance().getDocument(virtualFile)?.text
            ?: VfsUtilCore.loadText(virtualFile)

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
                    compiler?.let { CpRunPaths.resolveStdlibRoot(it)?.toString() },
                ).asSequence(),
            )
            .distinct()
            .toList()
    }

    private fun importedModules(text: String): List<String> =
        importRegex.findAll(text).map { it.groupValues[1] }.toList()

    private fun resolveImportPath(
        base: Path?,
        moduleName: String,
        roots: List<String>,
        sourceTexts: Map<String, String>,
    ): String? {
        val candidates = mutableListOf<Path>()
        if (base != null) {
            candidates.add(base.resolve(moduleLeafPath(moduleName)))
            candidates.add(base.resolve(moduleRelativePath(moduleName)))
            aggregateRelativePath(moduleName)?.let { candidates.add(base.resolve(it)) }
        }
        for (root in roots) {
            val rootPath = Path.of(root)
            candidates.add(rootPath.resolve(moduleRelativePath(moduleName)))
            aggregateRelativePath(moduleName)?.let { candidates.add(rootPath.resolve(it)) }
            stdlibRelativePath(moduleName)?.let { candidates.add(rootPath.resolve(it)) }
        }
        return candidates
            .asSequence()
            .map { normalizedPath(it.toString()) }
            .firstOrNull { it in sourceTexts || Files.isRegularFile(Path.of(it)) }
    }

    private fun moduleRelativePath(moduleName: String): Path {
        val components = moduleName.split(".")
        var path = Path.of("")
        for ((index, component) in components.withIndex()) {
            path = path.resolve(if (index == components.lastIndex) "$component.cp" else component)
        }
        return path
    }

    private fun moduleLeafPath(moduleName: String): Path =
        Path.of("${moduleName.substringAfterLast('.')}.cp")

    private fun aggregateRelativePath(moduleName: String): Path? {
        val components = moduleName.split(".")
        var path = Path.of("")
        for (component in components) {
            path = path.resolve(component)
        }
        return path.resolve("${components.last()}.cp")
    }

    private fun stdlibRelativePath(moduleName: String): Path? =
        moduleName.removePrefix("std.").takeIf { it != moduleName }?.let { moduleRelativePath(it) }

    private fun readText(path: String): String? =
        runCatching {
            Path.of(path).takeIf { Files.isRegularFile(it) }?.let { Files.readString(it) }
        }.getOrNull()

    private fun normalizedPath(path: String): String =
        Path.of(path).toAbsolutePath().normalize().toString()
}

private fun Path?.parentsFromSelf(): Sequence<Path> =
    generateSequence(this) { it.parent }
