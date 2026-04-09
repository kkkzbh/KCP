package org.cp.lang.clion

import com.intellij.openapi.diagnostic.Logger
import com.intellij.openapi.util.SystemInfo
import kotlinx.serialization.Serializable
import kotlinx.serialization.builtins.ListSerializer
import kotlinx.serialization.json.Json
import java.nio.charset.StandardCharsets
import java.nio.file.Files
import java.nio.file.Path
import java.util.concurrent.CompletableFuture
import java.util.concurrent.TimeUnit

@Serializable
data class CpHelperDiagnostic(
    val code: String,
    val message: String,
    val severity: String,
    val startOffset: Int,
    val endOffset: Int,
    val line: Int,
    val column: Int,
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
        runHelper("analyze", filename, text)?.let { payload ->
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
        runHelper("tokens", filename, text)?.let { payload ->
            val offsetMap = Utf8OffsetMap(text)
            json.decodeFromString(ListSerializer(CpHelperToken.serializer()), payload)
                .map { token ->
                    token.copy(
                        startOffset = offsetMap.toCharOffset(token.startOffset),
                        endOffset = offsetMap.toCharOffset(token.endOffset),
                    )
                }
        }.orEmpty()

    private fun runHelper(command: String, filename: String, text: String): String? {
        val helper = resolveHelperPath() ?: return null
        helper.toFile().setExecutable(true)

        return try {
            val process = ProcessBuilder(
                helper.toString(),
                command,
                "--stdin",
                "--filename",
                filename,
                "--format",
                "json",
            ).start()
            val stdoutFuture = process.readAsync(process.inputStream)
            val stderrFuture = process.readAsync(process.errorStream)

            process.outputStream.use { output ->
                output.write(text.toByteArray(StandardCharsets.UTF_8))
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

        val classLocation = runCatching {
            Path.of(CpHelperRunner::class.java.protectionDomain.codeSource.location.toURI())
        }.getOrNull() ?: return null

        val pluginRoot = when {
            Files.isRegularFile(classLocation) -> classLocation.parent?.parent
            else -> classLocation
        } ?: return null

        val helper = pluginRoot.resolve(CpPlugin.LINUX_HELPER_RELATIVE_PATH)
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
