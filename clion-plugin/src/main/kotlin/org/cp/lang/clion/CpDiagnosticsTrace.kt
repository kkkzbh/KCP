package org.cp.lang.clion

import com.intellij.openapi.diagnostic.Logger
import java.nio.file.Files
import java.nio.file.Path
import java.util.concurrent.ConcurrentHashMap

internal object CpDiagnosticsTrace {
    private val log = Logger.getInstance("org.cp.lang.clion.diagnostics")
    private val emittedKeys = ConcurrentHashMap.newKeySet<String>()
    private val traceFlag = Path.of(
        System.getProperty("cp.diagnostics.trace.flag", "/tmp/cp-clion-diagnostics-trace"),
    )

    fun isEnabled(): Boolean =
        runCatching { Files.exists(traceFlag) }.getOrDefault(false)

    fun info(key: String, message: () -> String) {
        if (isEnabled() && emittedKeys.add(key)) {
            log.info(message())
        }
    }

    fun warn(key: String, throwable: Throwable? = null, message: () -> String) {
        if (isEnabled() && emittedKeys.add(key)) {
            if (throwable == null) {
                log.warn(message())
            } else {
                log.warn(message(), throwable)
            }
        }
    }
}

internal fun CpInspectionResult.diagnosticSummary(): String =
    diagnostics.joinToString(
        separator = ",",
        prefix = "[",
        postfix = "]",
    ) { diagnostic -> "${diagnostic.code}@${diagnostic.startOffset}:${diagnostic.endOffset}" }

internal fun CpInspectionResult.navigationSummary(): String =
    navigation.take(8).joinToString(
        separator = ",",
        prefix = "[",
        postfix = if (navigation.size > 8) ",...]" else "]",
    ) { navigation ->
        "${navigation.category}@${navigation.sourceStartOffset}:${navigation.sourceEndOffset}->" +
            "${navigation.targetFile}:${navigation.targetStartOffset}:${navigation.targetEndOffset}"
    }
