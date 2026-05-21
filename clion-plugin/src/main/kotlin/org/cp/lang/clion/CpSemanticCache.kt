package org.cp.lang.clion

import com.intellij.codeInsight.daemon.DaemonCodeAnalyzer
import com.intellij.openapi.application.ApplicationManager
import com.intellij.openapi.fileEditor.FileDocumentManager
import com.intellij.openapi.progress.ProcessCanceledException
import com.intellij.openapi.project.Project
import com.intellij.openapi.util.Key
import com.intellij.openapi.vfs.LocalFileSystem
import com.intellij.openapi.vfs.VfsUtilCore
import com.intellij.openapi.vfs.VirtualFile
import com.intellij.openapi.vfs.VirtualFileManager
import com.intellij.psi.PsiFile
import com.intellij.psi.SmartPsiElementPointer
import com.intellij.psi.SmartPointerManager
import com.intellij.psi.util.PsiModificationTracker
import java.nio.file.Files
import java.nio.file.Path
import java.util.Collections
import java.util.concurrent.ConcurrentHashMap

internal data class CpCachedInspection(
    val request: CpInspectionRequest,
    val result: CpInspectionResult,
    val modificationCount: Long,
)

internal class CpSemanticCache private constructor(
    private val project: Project,
) {
    private val entries = ConcurrentHashMap<String, CpCachedInspection>()
    private val inFlight = Collections.newSetFromMap(ConcurrentHashMap<CpSemanticCacheKey, Boolean>())

    fun current(file: PsiFile): CpCachedInspection? {
        val path = file.cpSemanticPath() ?: return null
        val cached = entries[path] ?: return null
        if (cached.modificationCount == cpModificationCount(project)) {
            return cached
        }
        return cached.takeIf { it.request.sourcesMatchCurrentText() }
    }

    fun store(request: CpInspectionRequest, result: CpInspectionResult, modificationCount: Long) {
        CpDiagnosticsTrace.info("semantic-store:${request.activeFile}:${result.diagnosticSummary()}") {
            "cp semantic store active=${request.activeFile} accepted=${result.accepted} " +
                "diagnostics=${result.diagnosticSummary()} highlights=${result.highlights.size} " +
                "files=${request.files.map { it.path }} modificationCount=$modificationCount"
        }
        entries[cpNormalizePath(request.activeFile)] = CpCachedInspection(
            request = request,
            result = result,
            modificationCount = modificationCount,
        )
    }

    fun computeNow(file: PsiFile, activeText: String): CpCachedInspection? {
        val modificationCount = cpModificationCount(project)
        val request = CpProjectSnapshotCollector.collect(file, activeText) ?: return null
        val result = CpHelperRunner.inspectOrNull(request) ?: return null
        store(request, result, modificationCount)
        return current(file)
    }

    fun requestRefresh(file: PsiFile, activeText: String) {
        val activePath = file.cpSemanticPath() ?: return
        val modificationCount = cpModificationCount(project)
        val key = CpSemanticCacheKey(activePath, modificationCount)
        if (!inFlight.add(key)) {
            CpDiagnosticsTrace.info("semantic-refresh-duplicate:$activePath:$modificationCount") {
                "cp semantic refresh already in flight active=$activePath modificationCount=$modificationCount"
            }
            return
        }

        CpDiagnosticsTrace.info("semantic-refresh-start:$activePath:$modificationCount") {
            "cp semantic refresh start active=$activePath modificationCount=$modificationCount"
        }
        val pointer = SmartPointerManager.getInstance(project).createSmartPsiElementPointer(file)
        ApplicationManager.getApplication().executeOnPooledThread {
            try {
                val request = ApplicationManager.getApplication().runReadAction<CpInspectionRequest?> {
                    val psiFile = pointer.element ?: return@runReadAction null
                    val text = psiFile.activeDocumentText() ?: activeText
                    CpProjectSnapshotCollector.collect(psiFile, text)
                } ?: return@executeOnPooledThread
                val result = CpHelperRunner.inspectOrNull(request) ?: run {
                    entries.remove(activePath)
                    return@executeOnPooledThread
                }
                store(request, result, modificationCount)
                CpDiagnosticsTrace.info("semantic-refresh-done:$activePath:$modificationCount:${result.diagnosticSummary()}") {
                    "cp semantic refresh done active=$activePath accepted=${result.accepted} " +
                        "diagnostics=${result.diagnosticSummary()} highlights=${result.highlights.size}"
                }
                restartDaemon(pointer)
            } catch (exception: ProcessCanceledException) {
                throw exception
            } catch (exception: Exception) {
                CpDiagnosticsTrace.warn("semantic-refresh-failed:$activePath:$modificationCount", exception) {
                    "cp semantic refresh failed active=$activePath modificationCount=$modificationCount"
                }
                entries.remove(activePath)
            } finally {
                inFlight.remove(key)
            }
        }
    }

    private fun restartDaemon(pointer: SmartPsiElementPointer<PsiFile>) {
        if (ApplicationManager.getApplication().isUnitTestMode) {
            return
        }
        ApplicationManager.getApplication().invokeLater {
            if (project.isDisposed) {
                return@invokeLater
            }
            val psiFile = pointer.element ?: return@invokeLater
            CpDiagnosticsTrace.info("semantic-restart-daemon:${psiFile.virtualFile?.path ?: psiFile.name}") {
                "cp semantic restart daemon file=${psiFile.virtualFile?.path ?: psiFile.name}"
            }
            DaemonCodeAnalyzer.getInstance(project).restart(psiFile)
        }
    }

    companion object {
        private val cacheKey = Key.create<CpSemanticCache>("org.cp.lang.clion.semanticCache")

        fun get(project: Project): CpSemanticCache {
            project.getUserData(cacheKey)?.let { return it }
            return synchronized(project) {
                project.getUserData(cacheKey) ?: CpSemanticCache(project).also {
                    project.putUserData(cacheKey, it)
                }
            }
        }
    }
}

private data class CpSemanticCacheKey(
    val activePath: String,
    val modificationCount: Long,
)

internal fun cpModificationCount(project: Project): Long =
    PsiModificationTracker.getInstance(project).modificationCount

private fun PsiFile.cpSemanticPath(): String? =
    virtualFile?.path?.let(::cpNormalizePath)

private fun PsiFile.activeDocumentText(): String? =
    virtualFile?.let { FileDocumentManager.getInstance().getDocument(it)?.text }

private fun CpInspectionRequest.sourcesMatchCurrentText(): Boolean =
    files.all { source ->
        val currentText = findSnapshotVirtualFile(source.path)?.currentText()
            ?: runCatching { Files.readString(Path.of(source.path)) }.getOrNull()
        currentText != null && currentText == source.text
    }

private fun findSnapshotVirtualFile(path: String): VirtualFile? =
    VirtualFileManager.getInstance().findFileByUrl(path)
        ?: LocalFileSystem.getInstance().findFileByPath(path)
        ?: runCatching { LocalFileSystem.getInstance().findFileByNioFile(Path.of(path)) }.getOrNull()

private fun VirtualFile.currentText(): String? =
    FileDocumentManager.getInstance().getDocument(this)?.text
        ?: runCatching { VfsUtilCore.loadText(this) }.getOrNull()
