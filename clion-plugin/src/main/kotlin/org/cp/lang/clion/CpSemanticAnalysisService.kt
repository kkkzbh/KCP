package org.cp.lang.clion

import com.intellij.openapi.project.Project
import com.intellij.openapi.util.Key
import java.security.MessageDigest
import java.util.concurrent.ConcurrentHashMap
import java.util.concurrent.atomic.AtomicInteger

internal class CpSemanticAnalysisService private constructor() {
    private val inspectCache = ConcurrentHashMap<CpInspectionRequestKey, CpInspectionResult>()
    private val helperInspectCalls = AtomicInteger()
    private val inspectCacheHits = AtomicInteger()
    private val inspectCacheMisses = AtomicInteger()

    fun inspect(request: CpInspectionRequest, reason: String): CpInspectionResult? {
        val key = request.cacheKey()
        inspectCache[key]?.let { result ->
            inspectCacheHits.incrementAndGet()
            CpDiagnosticsTrace.info("semantic-inspect-cache-hit:$reason:${request.activeFile}:${key.fingerprint}") {
                "cp semantic inspect cache hit reason=$reason active=${request.activeFile} " +
                    "files=${request.files.size} bytes=${request.totalBytes()}"
            }
            return result
        }

        inspectCacheMisses.incrementAndGet()
        helperInspectCalls.incrementAndGet()
        val start = System.nanoTime()
        val result = CpHelperRunner.inspectOrNull(request) ?: return null
        val elapsedMillis = (System.nanoTime() - start) / 1_000_000
        CpDiagnosticsTrace.info("semantic-inspect-helper:$reason:${request.activeFile}:${key.fingerprint}") {
            "cp semantic inspect helper reason=$reason active=${request.activeFile} " +
                "files=${request.files.size} bytes=${request.totalBytes()} elapsedMs=$elapsedMillis " +
                "accepted=${result.accepted} diagnostics=${result.diagnosticSummary()} " +
                "navigation=${result.navigation.size}"
        }
        if (inspectCache.size > maxInspectCacheEntries) {
            inspectCache.clear()
        }
        inspectCache[key] = result
        return result
    }

    fun resetStats(clearCache: Boolean = false) {
        helperInspectCalls.set(0)
        inspectCacheHits.set(0)
        inspectCacheMisses.set(0)
        if (clearCache) {
            inspectCache.clear()
        }
    }

    fun stats(): CpSemanticAnalysisStats =
        CpSemanticAnalysisStats(
            helperInspectCalls = helperInspectCalls.get(),
            inspectCacheHits = inspectCacheHits.get(),
            inspectCacheMisses = inspectCacheMisses.get(),
            inspectCacheEntries = inspectCache.size,
        )

    companion object {
        private val cacheKey = Key.create<CpSemanticAnalysisService>("org.cp.lang.clion.semanticAnalysisService")
        private const val maxInspectCacheEntries = 128

        fun get(project: Project): CpSemanticAnalysisService {
            project.getUserData(cacheKey)?.let { return it }
            return synchronized(project) {
                project.getUserData(cacheKey) ?: CpSemanticAnalysisService().also {
                    project.putUserData(cacheKey, it)
                }
            }
        }
    }
}

internal data class CpSemanticAnalysisStats(
    val helperInspectCalls: Int,
    val inspectCacheHits: Int,
    val inspectCacheMisses: Int,
    val inspectCacheEntries: Int,
)

private fun CpInspectionRequest.cacheKey(): CpInspectionRequestKey =
    CpInspectionRequestKey(
        activeFile = activeFile,
        files = files.map { file ->
            CpInspectionRequestFileKey(
                path = file.path,
                textLength = file.text.length,
                textHash = file.text.sha256(),
            )
        },
    )

private fun CpInspectionRequest.totalBytes(): Int =
    files.sumOf { it.text.toByteArray().size }

private data class CpInspectionRequestKey(
    val activeFile: String,
    val files: List<CpInspectionRequestFileKey>,
) {
    val fingerprint: Int = files.fold(activeFile.hashCode()) { hash, file ->
        31 * hash + file.hashCode()
    }
}

private data class CpInspectionRequestFileKey(
    val path: String,
    val textLength: Int,
    val textHash: String,
)

private fun String.sha256(): String {
    val digest = MessageDigest.getInstance("SHA-256").digest(toByteArray())
    return digest.joinToString(separator = "") { byte -> "%02x".format(byte.toInt() and 0xff) }
}
