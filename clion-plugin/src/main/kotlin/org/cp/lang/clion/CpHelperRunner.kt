package org.cp.lang.clion

import com.intellij.openapi.diagnostic.Logger
import com.intellij.openapi.util.SystemInfo
import kotlinx.serialization.Serializable
import kotlinx.serialization.builtins.ListSerializer
import kotlinx.serialization.encodeToString
import kotlinx.serialization.json.Json
import java.nio.charset.StandardCharsets
import java.nio.file.Files
import java.nio.file.Path
import java.util.concurrent.CompletableFuture
import java.util.concurrent.TimeUnit

@Serializable
data class CpHelperDiagnostic(
    val stage: String = "",
    val code: String,
    val message: String,
    val severity: String,
    val startOffset: Int,
    val endOffset: Int,
    val line: Int,
    val column: Int,
)

@Serializable
data class CpHelperHighlight(
    val category: String,
    val startOffset: Int,
    val endOffset: Int,
)

@Serializable
data class CpHelperNavigation(
    val category: String,
    val sourceStartOffset: Int,
    val sourceEndOffset: Int,
    val targetFile: String,
    val targetStartOffset: Int,
    val targetEndOffset: Int,
)

@Serializable
data class CpHelperCapture(
    val name: String,
    val mode: String,
    val reason: String,
    val referenceStartOffset: Int,
    val referenceEndOffset: Int,
    val sourceStartOffset: Int,
    val sourceEndOffset: Int,
    val mutated: Boolean,
    val escaped: Boolean,
)

@Serializable
data class CpInspectionFile(
    val path: String,
    val text: String,
)

@Serializable
data class CpInspectionRequest(
    val activeFile: String,
    val files: List<CpInspectionFile>,
)

@Serializable
data class CpInspectionResolveRequest(
    val activeFile: String,
    val targetFile: String = activeFile,
    val entryFiles: List<String> = emptyList(),
    val importRoots: List<String> = emptyList(),
    val searchRoots: List<String> = emptyList(),
    val files: List<CpInspectionFile> = emptyList(),
    val followStdlibImports: Boolean = true,
)

@Serializable
data class CpInspectionResult(
    val accepted: Boolean,
    val diagnostics: List<CpHelperDiagnostic>,
    val highlights: List<CpHelperHighlight>,
    val captures: List<CpHelperCapture> = emptyList(),
    val navigation: List<CpHelperNavigation> = emptyList(),
)

@Serializable
data class CpHelperToken(
    val kind: String,
    val lexeme: String,
    val startOffset: Int,
    val endOffset: Int,
    val leadingSpace: Boolean,
    val startOfLine: Boolean,
    val unterminated: Boolean,
    val recovered: Boolean,
)

object CpHelperRunner {
    private val log: Logger = Logger.getInstance(CpHelperRunner::class.java)
    private val json = Json { ignoreUnknownKeys = true }

    fun analyze(filename: String, text: String): List<CpHelperDiagnostic> =
        runHelper(
            command = "analyze",
            stdin = text,
            filename = filename,
        )?.let { payload ->
            val offsetMap = Utf8OffsetMap(text)
            json.decodeFromString(ListSerializer(CpHelperDiagnostic.serializer()), payload)
                .map { diagnostic ->
                    val startOffset = offsetMap.toCharOffset(diagnostic.startOffset)
                    val endOffset = offsetMap.toCharOffset(diagnostic.endOffset)
                    val (line, column) = offsetMap.lineAndColumn(startOffset)
                    diagnostic.copy(
                        startOffset = startOffset,
                        endOffset = endOffset,
                        line = line,
                        column = column,
                    )
                }
        }.orEmpty()

    fun tokens(filename: String, text: String): List<CpHelperToken> =
        runHelper(
            command = "tokens",
            stdin = text,
            filename = filename,
        )?.let { payload ->
            val offsetMap = Utf8OffsetMap(text)
            json.decodeFromString(ListSerializer(CpHelperToken.serializer()), payload)
                .map { token ->
                    token.copy(
                        startOffset = offsetMap.toCharOffset(token.startOffset),
                        endOffset = offsetMap.toCharOffset(token.endOffset),
                    )
                }
        }.orEmpty()

    fun inspect(request: CpInspectionRequest): CpInspectionResult =
        inspectOrNull(request) ?: emptyInspectionResult()

    fun resolveInspectionRequest(request: CpInspectionResolveRequest): CpInspectionRequest? =
        runHelper(
            command = "resolve",
            stdin = json.encodeToString(request),
        )?.let { payload ->
            json.decodeFromString(CpInspectionRequest.serializer(), payload)
        }

    fun inspectOrNull(request: CpInspectionRequest): CpInspectionResult? {
        CpDiagnosticsTrace.info("helper-inspect-start:${request.activeFile}:${request.files.map { it.path }}") {
            "cp helper inspect start active=${request.activeFile} files=${request.files.map { it.path }}"
        }
        val activeText = request.files.firstOrNull { it.path == request.activeFile }?.text.orEmpty()
        val offsetMap = Utf8OffsetMap(activeText)
        val sourceTexts = request.files.associate { it.path to it.text }
        val targetOffsetMaps = mutableMapOf<String, Utf8OffsetMap>()
        fun targetOffsetMap(path: String): Utf8OffsetMap? =
            targetOffsetMaps.getOrPut(path) {
                Utf8OffsetMap(sourceTexts[path] ?: return null)
            }
        val payload = json.encodeToString(request)
        return runHelper(
            command = "inspect",
            stdin = payload,
        )?.let { output ->
            val decoded = json.decodeFromString(CpInspectionResult.serializer(), output)
            decoded.copy(
                diagnostics = decoded.diagnostics.map { diagnostic ->
                    val startOffset = offsetMap.toCharOffset(diagnostic.startOffset)
                    val endOffset = offsetMap.toCharOffset(diagnostic.endOffset)
                    val (line, column) = offsetMap.lineAndColumn(startOffset)
                    diagnostic.copy(
                        startOffset = startOffset,
                        endOffset = endOffset,
                        line = line,
                        column = column,
                    )
                },
                highlights = decoded.highlights.map { highlight ->
                    highlight.copy(
                        startOffset = offsetMap.toCharOffset(highlight.startOffset),
                        endOffset = offsetMap.toCharOffset(highlight.endOffset),
                    )
                },
                navigation = decoded.navigation.map { navigation ->
                    val targetOffsetMap = targetOffsetMap(navigation.targetFile)
                    navigation.copy(
                        sourceStartOffset = offsetMap.toCharOffset(navigation.sourceStartOffset),
                        sourceEndOffset = offsetMap.toCharOffset(navigation.sourceEndOffset),
                        targetStartOffset = targetOffsetMap?.toCharOffset(navigation.targetStartOffset)
                            ?: navigation.targetStartOffset,
                        targetEndOffset = targetOffsetMap?.toCharOffset(navigation.targetEndOffset)
                            ?: navigation.targetEndOffset,
                    )
                },
            ).also { result ->
                CpDiagnosticsTrace.info("helper-inspect-done:${request.activeFile}:${result.diagnosticSummary()}") {
                    "cp helper inspect done active=${request.activeFile} accepted=${result.accepted} " +
                        "diagnostics=${result.diagnosticSummary()} highlights=${result.highlights.size} " +
                        "navigation=${result.navigation.size} ${result.navigationSummary()}"
                }
            }
        }
    }

    private fun emptyInspectionResult(): CpInspectionResult =
        CpInspectionResult(
            accepted = false,
            diagnostics = emptyList(),
            highlights = emptyList(),
            navigation = emptyList(),
        )

    private fun runHelper(command: String, stdin: String, filename: String? = null): String? {
        val helper = resolveHelperPath() ?: run {
            CpDiagnosticsTrace.warn("helper-missing:$command") {
                "cp helper missing for command=$command"
            }
            return null
        }
        helper.toFile().setExecutable(true)
        CpDiagnosticsTrace.info("helper-command:$command:$helper") {
            "cp helper command=$command path=$helper"
        }

        return try {
            val arguments = mutableListOf(
                helper.toString(),
                command,
                "--stdin",
                "--format",
                "json",
            )
            if (filename != null) {
                arguments.addAll(listOf("--filename", filename))
            }
            val process = ProcessBuilder(arguments).start()
            val stdoutFuture = process.readAsync(process.inputStream)
            val stderrFuture = process.readAsync(process.errorStream)

            process.outputStream.use { output ->
                output.write(stdin.toByteArray(StandardCharsets.UTF_8))
            }

            val finished = process.waitFor(30, TimeUnit.SECONDS)
            if (!finished) {
                process.destroyForcibly()
                process.waitFor(5, TimeUnit.SECONDS)
                log.warn("cp helper timed out while running $command")
                return null
            }

            val stdout = stdoutFuture.get(5, TimeUnit.SECONDS)
            val stderr = stderrFuture.get(5, TimeUnit.SECONDS)
            if (process.exitValue() != 0) {
                log.warn("cp helper failed for $command: $stderr")
                return null
            }

            stdout
        } catch (exception: Exception) {
            log.warn("failed to invoke cp helper", exception)
            null
        }
    }

    private fun resolveHelperPath(): Path? {
        val explicit = System.getProperty("cp.helper.path")
        if (!explicit.isNullOrBlank()) {
            val path = Path.of(explicit)
            if (Files.isRegularFile(path)) {
                return path
            }
        }

        if (!SystemInfo.isLinux) {
            return null
        }

        val helper = cpPluginNativePath("kcp-lexer-helper") ?: return null
        CpDiagnosticsTrace.info("helper-classpath-candidate:$helper") {
            "cp helper packaged candidate=$helper exists=${Files.isRegularFile(helper)}"
        }
        return helper.takeIf(Files::isRegularFile)
    }
}

private fun Process.readAsync(stream: java.io.InputStream): CompletableFuture<String> =
    CompletableFuture.supplyAsync {
        stream.bufferedReader(StandardCharsets.UTF_8).use { it.readText() }
    }

private class Utf8OffsetMap(text: String) {
    private val byteToCharOffset: IntArray
    private val lineStarts: IntArray

    init {
        byteToCharOffset = buildByteToCharOffset(text)
        lineStarts = buildLineStarts(text)
    }

    fun toCharOffset(byteOffset: Int): Int {
        val index = byteOffset.coerceIn(0, byteToCharOffset.lastIndex)
        return byteToCharOffset[index]
    }

    fun lineAndColumn(charOffset: Int): Pair<Int, Int> {
        val boundedOffset = charOffset.coerceAtLeast(0)
        val lineIndex = lineStarts.binarySearch(boundedOffset).let { index ->
            if (index >= 0) {
                index
            } else {
                (-index - 2).coerceAtLeast(0)
            }
        }
        return (lineIndex + 1) to (boundedOffset - lineStarts[lineIndex] + 1)
    }

    private fun buildByteToCharOffset(text: String): IntArray {
        val totalBytes = text.toByteArray(StandardCharsets.UTF_8).size
        val mapping = IntArray(totalBytes + 1)

        var charIndex = 0
        var byteIndex = 0
        while (charIndex < text.length) {
            val codePoint = text.codePointAt(charIndex)
            val charCount = Character.charCount(codePoint)
            val byteCount = utf8Length(codePoint)
            mapping[byteIndex] = charIndex
            for (delta in 1 until byteCount) {
                mapping[byteIndex + delta] = charIndex
            }
            charIndex += charCount
            byteIndex += byteCount
            mapping[byteIndex] = charIndex
        }

        mapping[totalBytes] = text.length
        return mapping
    }

    private fun buildLineStarts(text: String): IntArray {
        val starts = ArrayList<Int>()
        starts += 0
        for (index in text.indices) {
            if (text[index] == '\n' && index + 1 <= text.length) {
                starts += index + 1
            }
        }
        return starts.toIntArray()
    }

    private fun utf8Length(codePoint: Int): Int =
        when {
            codePoint <= 0x7F -> 1
            codePoint <= 0x7FF -> 2
            codePoint <= 0xFFFF -> 3
            else -> 4
        }
}
