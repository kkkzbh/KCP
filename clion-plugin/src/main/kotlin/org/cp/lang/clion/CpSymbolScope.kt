package org.cp.lang.clion

import com.intellij.openapi.fileEditor.FileDocumentManager
import com.intellij.openapi.project.Project
import com.intellij.openapi.util.Key
import com.intellij.openapi.vfs.LocalFileSystem
import com.intellij.psi.PsiFile
import com.intellij.psi.PsiFileFactory
import com.intellij.psi.PsiManager
import java.nio.file.Path

internal data class CpSymbolScopeSnapshot(
    val entries: List<CpSymbolScopeEntry>,
)

internal data class CpSymbolScopeEntry(
    val path: String,
    val file: PsiFile,
    val symbols: CpFileSymbols,
    val stamp: CpSymbolScopeStamp,
)

internal data class CpSymbolScopeStamp(
    val path: String,
    val modificationStamp: Long,
    val textHash: Int?,
)

internal object CpSymbolScope {
    fun files(file: PsiFile): List<PsiFile> =
        snapshot(file).entries.map { it.file }

    fun snapshot(file: PsiFile): CpSymbolScopeSnapshot {
        val sources = linkedMapOf<String, CpSymbolScopeSource>()
        val activePath = file.virtualFile?.path ?: file.name
        val normalizedActivePath = normalize(activePath)
        sources[normalizedActivePath] = CpSymbolScopeSource(
            path = normalizedActivePath,
            file = file,
            textHash = null,
        )

        val cached = CpSemanticCache.get(file.project).current(file)
            ?: CpSemanticCache.get(file.project).latest(file)
        for (source in cached?.request?.files.orEmpty()) {
            val path = normalize(source.path)
            if (sources.containsKey(path)) {
                continue
            }
            sources[path] = CpSymbolScopeSource(
                path = path,
                file = source.psiFile(file.project),
                textHash = source.text.hashCode(),
            )
        }
        return CpSymbolScopeSnapshot(
            entries = sources.values.map { source ->
                CpSymbolScopeEntry(
                    path = source.path,
                    file = source.file,
                    symbols = CpFileSymbolIndex.get(source.file),
                    stamp = CpSymbolScopeStamp(
                        path = source.path,
                        modificationStamp = source.file.modificationStamp,
                        textHash = source.textHash,
                    ),
                )
            },
        )
    }

    private fun CpInspectionFile.psiFile(project: Project): PsiFile {
        val local = LocalFileSystem.getInstance().findFileByPath(path)
        if (local != null) {
            val document = FileDocumentManager.getInstance().getDocument(local)
            if (document == null || document.text == text) {
                PsiManager.getInstance(project).findFile(local)?.let { return it }
            }
        }
        return CpSnapshotPsiFileCache.get(project, this)
    }

    private fun normalize(path: String): String =
        runCatching { Path.of(path).toAbsolutePath().normalize().toString() }.getOrDefault(path)
}

private object CpSnapshotPsiFileCache {
    fun get(project: Project, source: CpInspectionFile): PsiFile {
        val cache = project.getUserData(cacheKey) ?: mutableMapOf<String, CachedSnapshotPsiFile>().also {
            project.putUserData(cacheKey, it)
        }
        val cached = cache[source.path]
        if (cached != null && cached.text == source.text && cached.file.isValid) {
            return cached.file
        }

        val file = PsiFileFactory.getInstance(project).createFileFromText(
            Path.of(source.path).fileName?.toString() ?: "snapshot.cp",
            CpFileType.INSTANCE,
            source.text,
        )
        cache[source.path] = CachedSnapshotPsiFile(source.text, file)
        return file
    }

    private val cacheKey = Key.create<MutableMap<String, CachedSnapshotPsiFile>>("org.cp.lang.clion.snapshotPsiFileCache")
}

private data class CachedSnapshotPsiFile(
    val text: String,
    val file: PsiFile,
)

private data class CpSymbolScopeSource(
    val path: String,
    val file: PsiFile,
    val textHash: Int?,
)
