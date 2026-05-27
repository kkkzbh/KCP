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
import java.util.Collections
import java.util.concurrent.ConcurrentHashMap

internal data class CpCachedInspection(
    val request: CpInspectionRequest,
    val result: CpInspectionResult,
    val modificationCount: Long,
    val signature: List<CpInspectionFileSignature>,
    val presentationFingerprint: CpInspectionPresentationFingerprint,
)

internal data class CpSemanticStoreUpdate(
    val presentationChanged: Boolean,
)

internal data class CpInspectionPresentationFingerprint(
    val diagnostics: List<CpDiagnosticPresentationFingerprint>,
    val highlights: List<CpHighlightPresentationFingerprint>,
)

internal data class CpDiagnosticPresentationFingerprint(
    val severity: String,
    val stage: String,
    val code: String,
    val message: String,
    val startOffset: Int,
    val endOffset: Int,
)

internal data class CpHighlightPresentationFingerprint(
    val category: String,
    val startOffset: Int,
    val endOffset: Int,
)

internal class CpSemanticCache private constructor(
    private val project: Project,
) {
    private val entries = ConcurrentHashMap<String, CpCachedInspection>()
    private val inFlight = Collections.newSetFromMap(ConcurrentHashMap<CpSemanticCacheKey, Boolean>())

    fun current(file: PsiFile): CpCachedInspection? =
        readAction {
            val path = file.cpSemanticPath() ?: return@readAction null
            val cached = entries[path] ?: return@readAction null
            cached.takeIf { it.isCurrentFor(file) }
        }

    fun latest(file: PsiFile): CpCachedInspection? =
        readAction {
            val path = file.cpSemanticPath() ?: return@readAction null
            entries[path]
        }

    fun presentation(file: PsiFile): CpInspectionResult? =
        readAction {
            val path = file.cpSemanticPath() ?: return@readAction null
            val cached = entries[path] ?: return@readAction null
            if (cached.isCurrentFor(file)) {
                return@readAction cached.result
            }
            cached.projectedPresentationFor(file)
        }

    fun store(request: CpInspectionRequest, result: CpInspectionResult, modificationCount: Long): CpSemanticStoreUpdate {
        CpDiagnosticsTrace.info("semantic-store:${request.activeFile}:${result.diagnosticSummary()}") {
            "cp semantic store active=${request.activeFile} accepted=${result.accepted} " +
                "diagnostics=${result.diagnosticSummary()} highlights=${result.highlights.size} " +
                "files=${request.files.map { it.path }} modificationCount=$modificationCount"
        }
        val activePath = cpNormalizePath(request.activeFile)
        val previous = entries[activePath]
        val next = CpCachedInspection(
            request = request,
            result = result,
            modificationCount = modificationCount,
            signature = request.files.map { it.signature() },
            presentationFingerprint = result.presentationFingerprint(),
        )
        entries[activePath] = next
        return CpSemanticStoreUpdate(
            presentationChanged = previous?.presentationFingerprint != next.presentationFingerprint,
        )
    }

    fun computeNow(file: PsiFile, activeText: String): CpCachedInspection? {
        val modificationCount = cpModificationCount(project)
        val request = CpProjectSnapshotCollector.collect(file, activeText) ?: return null
        val result = CpHelperRunner.inspectOrNull(request) ?: return null
        store(request, result, modificationCount)
        return current(file)
    }

    fun requestRefresh(file: PsiFile, force: Boolean = false) {
        val activePath = file.cpSemanticPath() ?: return
        if (!force && ApplicationManager.getApplication().runReadAction<Boolean> { current(file) != null }) {
            CpDiagnosticsTrace.info("semantic-refresh-current:$activePath") {
                "cp semantic refresh skipped because cache is current active=$activePath"
            }
            return
        }
        val activeSignature = ApplicationManager.getApplication().runReadAction<CpActiveTextSignature?> {
            if (project.isDisposed || !file.isValid) {
                return@runReadAction null
            }
            file.activeTextSignature()
        } ?: return
        val key = CpSemanticCacheKey(activePath, activeSignature)
        if (!inFlight.add(key)) {
            CpDiagnosticsTrace.info("semantic-refresh-duplicate:$activePath:$activeSignature") {
                "cp semantic refresh already in flight active=$activePath activeSignature=$activeSignature"
            }
            return
        }

        CpDiagnosticsTrace.info("semantic-refresh-start:$activePath:$activeSignature") {
            "cp semantic refresh start active=$activePath activeSignature=$activeSignature force=$force"
        }
        val pointer = SmartPointerManager.getInstance(project).createSmartPsiElementPointer(file)
        ApplicationManager.getApplication().executeOnPooledThread {
            try {
                val context = ApplicationManager.getApplication().runReadAction<CpSnapshotContext?> {
                    val psiFile = pointer.element ?: return@runReadAction null
                    if (project.isDisposed || !psiFile.isValid || psiFile.activeTextSignature() != activeSignature) {
                        return@runReadAction null
                    }
                    val text = psiFile.activeDocumentText() ?: psiFile.text
                    CpProjectSnapshotCollector.prepare(psiFile, text)
                } ?: return@executeOnPooledThread
                if (refreshIsStale(pointer, activeSignature)) {
                    return@executeOnPooledThread
                }
                val request = context.resolve()
                if (refreshIsStale(pointer, activeSignature)) {
                    return@executeOnPooledThread
                }
                val result = CpHelperRunner.inspectOrNull(request) ?: run {
                    CpDiagnosticsTrace.warn("semantic-refresh-null:$activePath:$activeSignature") {
                        "cp semantic refresh produced no result; keeping previous cache active=$activePath"
                    }
                    return@executeOnPooledThread
                }
                if (refreshIsStale(pointer, activeSignature)) {
                    return@executeOnPooledThread
                }
                val update = store(request, result, cpModificationCount(project))
                CpDiagnosticsTrace.info("semantic-refresh-done:$activePath:$activeSignature:${result.diagnosticSummary()}") {
                    "cp semantic refresh done active=$activePath accepted=${result.accepted} " +
                        "diagnostics=${result.diagnosticSummary()} highlights=${result.highlights.size} " +
                        "presentationChanged=${update.presentationChanged}"
                }
                if (update.presentationChanged) {
                    restartDaemon(pointer)
                }
            } catch (exception: ProcessCanceledException) {
                throw exception
            } catch (exception: Exception) {
                CpDiagnosticsTrace.warn("semantic-refresh-failed:$activePath:$activeSignature", exception) {
                    "cp semantic refresh failed active=$activePath activeSignature=$activeSignature"
                }
            } finally {
                inFlight.remove(key)
            }
        }
    }

    private fun refreshIsStale(pointer: SmartPsiElementPointer<PsiFile>, activeSignature: CpActiveTextSignature): Boolean =
        project.isDisposed ||
            ApplicationManager.getApplication().runReadAction<Boolean> {
                val psiFile = pointer.element ?: return@runReadAction true
                !psiFile.isValid || psiFile.activeTextSignature() != activeSignature
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
    val activeSignature: CpActiveTextSignature,
)

private data class CpActiveTextSignature(
    val textLength: Int,
    val textHash: Int,
)

internal fun cpModificationCount(project: Project): Long =
    PsiModificationTracker.getInstance(project).modificationCount

private fun <T> readAction(block: () -> T): T {
    val application = ApplicationManager.getApplication()
    if (application.isReadAccessAllowed) {
        return block()
    }
    return application.runReadAction<T>(block)
}

private fun PsiFile.cpSemanticPath(): String? =
    virtualFile?.path?.let(::cpNormalizePath)

private fun PsiFile.activeDocumentText(): String? =
    virtualFile?.let { FileDocumentManager.getInstance().getDocument(it)?.text }

private fun PsiFile.activeTextSignature(): CpActiveTextSignature {
    val text = activeDocumentText() ?: this.text
    return CpActiveTextSignature(
        textLength = text.length,
        textHash = text.hashCode(),
    )
}

private fun CpCachedInspection.isCurrentFor(file: PsiFile): Boolean {
    val activePath = file.cpSemanticPath() ?: return false
    if (cpNormalizePath(request.activeFile) != activePath) {
        return false
    }
    return signature.all { it.matches(file) }
}

private fun CpCachedInspection.projectedPresentationFor(file: PsiFile): CpInspectionResult? {
    val activePath = file.cpSemanticPath() ?: return null
    if (cpNormalizePath(request.activeFile) != activePath) {
        return null
    }
    if (!signature.filterNot { it.path == activePath }.all { it.matches(file) }) {
        return null
    }
    val oldText = request.files
        .firstOrNull { cpNormalizePath(it.path) == activePath }
        ?.text
        ?: return null
    val newText = file.activeDocumentText() ?: file.text
    if (oldText == newText) {
        return result
    }
    return result.projectPresentation(oldText, newText)
}

private fun CpInspectionResult.projectPresentation(oldText: String, newText: String): CpInspectionResult {
    val projection = CpTextProjection.between(oldText, newText)
    return copy(
        diagnostics = diagnostics.mapNotNull { diagnostic ->
            projection.mapRange(diagnostic.startOffset, diagnostic.endOffset)?.let { range ->
                diagnostic.copy(
                    startOffset = range.first,
                    endOffset = range.second,
                )
            }
        },
        highlights = highlights.mapNotNull { highlight ->
            projection.mapRange(highlight.startOffset, highlight.endOffset)?.let { range ->
                highlight.copy(
                    startOffset = range.first,
                    endOffset = range.second,
                )
            }
        },
        navigation = emptyList(),
    )
}

private data class CpTextProjection(
    val oldChangeStart: Int,
    val oldChangeEnd: Int,
    val delta: Int,
) {
    fun mapRange(startOffset: Int, endOffset: Int): Pair<Int, Int>? =
        when {
            endOffset <= oldChangeStart -> startOffset to endOffset
            startOffset >= oldChangeEnd -> startOffset + delta to endOffset + delta
            else -> null
        }

    companion object {
        fun between(oldText: String, newText: String): CpTextProjection {
            var prefix = 0
            val prefixLimit = minOf(oldText.length, newText.length)
            while (prefix < prefixLimit && oldText[prefix] == newText[prefix]) {
                prefix += 1
            }

            var suffix = 0
            val suffixLimit = minOf(oldText.length - prefix, newText.length - prefix)
            while (
                suffix < suffixLimit &&
                    oldText[oldText.length - suffix - 1] == newText[newText.length - suffix - 1]
            ) {
                suffix += 1
            }

            return CpTextProjection(
                oldChangeStart = prefix,
                oldChangeEnd = oldText.length - suffix,
                delta = newText.length - oldText.length,
            )
        }
    }
}

private fun CpInspectionResult.presentationFingerprint(): CpInspectionPresentationFingerprint =
    CpInspectionPresentationFingerprint(
        diagnostics = diagnostics
            .map { diagnostic ->
                CpDiagnosticPresentationFingerprint(
                    severity = diagnostic.severity,
                    stage = diagnostic.stage,
                    code = diagnostic.code,
                    message = diagnostic.message,
                    startOffset = diagnostic.startOffset,
                    endOffset = diagnostic.endOffset,
                )
            }
            .distinct()
            .sortedWith(
                compareBy<CpDiagnosticPresentationFingerprint> { it.startOffset }
                    .thenBy { it.endOffset }
                    .thenBy { it.severity }
                    .thenBy { it.stage }
                    .thenBy { it.code }
                    .thenBy { it.message },
            ),
        highlights = highlights
            .map { highlight ->
                CpHighlightPresentationFingerprint(
                    category = highlight.category,
                    startOffset = highlight.startOffset,
                    endOffset = highlight.endOffset,
                )
            }
            .distinct()
            .sortedWith(
                compareBy<CpHighlightPresentationFingerprint> { it.startOffset }
                    .thenBy { it.endOffset }
                    .thenBy { it.category },
            ),
    )

private fun CpInspectionFile.signature(): CpInspectionFileSignature =
    findInspectionVirtualFile(cpNormalizePath(path)).let { virtualFile ->
        CpInspectionFileSignature(
            path = cpNormalizePath(path),
            textLength = text.length,
            textHash = text.hashCode(),
            virtualFileStamp = virtualFile?.modificationStamp,
            virtualFileLength = virtualFile?.length,
        )
    }

private fun CpInspectionFileSignature.matches(activeFile: PsiFile): Boolean {
    if (activeFile.cpSemanticPath() == path) {
        val text = activeFile.activeDocumentText() ?: activeFile.text
        return text.length == textLength && text.hashCode() == textHash
    }

    val virtualFile = findInspectionVirtualFile(path) ?: return true
    val document = FileDocumentManager.getInstance().getDocument(virtualFile)
    if (document != null) {
        return document.text.let { text -> text.length == textLength && text.hashCode() == textHash }
    }
    if (
        virtualFileStamp != null &&
        virtualFileLength != null &&
        virtualFile.modificationStamp == virtualFileStamp &&
        virtualFile.length == virtualFileLength
    ) {
        return true
    }

    return VfsUtilCore.loadText(virtualFile).let { text -> text.length == textLength && text.hashCode() == textHash }
}

private fun findInspectionVirtualFile(path: String): VirtualFile? =
    LocalFileSystem.getInstance().findFileByPath(path)
        ?: VirtualFileManager.getInstance().findFileByUrl(path)
        ?: VirtualFileManager.getInstance().findFileByUrl("temp://$path")

internal data class CpInspectionFileSignature(
    val path: String,
    val textLength: Int,
    val textHash: Int,
    val virtualFileStamp: Long?,
    val virtualFileLength: Long?,
)
