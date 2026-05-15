package org.cp.lang.clion

import com.intellij.openapi.diagnostic.Logger
import com.intellij.openapi.vfs.VfsUtilCore
import com.intellij.psi.PsiFile
import com.intellij.psi.search.FileTypeIndex
import com.intellij.psi.search.GlobalSearchScope

data class CpSnapshotSource(
    val path: String,
    val text: String,
)

object CpProjectSnapshotCollector {
    private val log: Logger = Logger.getInstance(CpProjectSnapshotCollector::class.java)

    fun collect(file: PsiFile, activeText: String): CpInspectionRequest? {
        if (file.language != CpLanguage) {
            return null
        }

        val activePath = file.virtualFile?.path ?: file.name
        val projectFiles = runCatching {
            FileTypeIndex.getFiles(CpFileType.INSTANCE, GlobalSearchScope.projectScope(file.project))
                .asSequence()
                .filter { it.isValid && !it.isDirectory }
                .mapNotNull { virtualFile ->
                    runCatching {
                        CpSnapshotSource(
                            path = virtualFile.path,
                            text = VfsUtilCore.loadText(virtualFile),
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
            projectFiles = projectFiles,
        )
    }

    fun buildRequest(
        activePath: String,
        activeText: String,
        projectFiles: Iterable<CpSnapshotSource>,
    ): CpInspectionRequest {
        val files = linkedMapOf<String, String>()
        for (projectFile in projectFiles) {
            files[projectFile.path] = projectFile.text
        }
        files[activePath] = activeText

        return CpInspectionRequest(
            activeFile = activePath,
            files = files.map { (path, text) ->
                CpInspectionFile(path = path, text = text)
            },
        )
    }
}
