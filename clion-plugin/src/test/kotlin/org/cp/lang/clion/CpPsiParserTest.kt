package org.cp.lang.clion

import com.intellij.lang.LanguageParserDefinitions
import com.intellij.testFramework.fixtures.BasePlatformTestCase
import com.intellij.psi.PsiElement
import com.intellij.psi.PsiErrorElement
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import java.nio.file.Files
import java.nio.file.Path
import kotlin.io.path.isRegularFile

class CpPsiParserTest : BasePlatformTestCase() {
    private val parserDefinition = CpParserDefinition()

    override fun setUp() {
        super.setUp()
        LanguageParserDefinitions.INSTANCE.addExplicitExtension(CpLanguage, parserDefinition)
    }

    override fun tearDown() {
        try {
            LanguageParserDefinitions.INSTANCE.removeExplicitExtension(CpLanguage, parserDefinition)
        } finally {
            super.tearDown()
        }
    }

    fun testRepresentativeSourceBuildsStructuredPsi() {
        val file = parse(
            """
            export module math;
            import std.io;

            export add(x: i32, y: i32) -> i32
            {
                let sum: i32 = x + y;
                return sum;
            }
            """.trimIndent(),
        )

        assertEquals(1, file.descendants(CpElements.MODULE_HEADER).size)
        assertEquals(1, file.descendants(CpElements.IMPORT_DECLARATION).size)
        assertEquals(1, file.descendants(CpElements.FUNCTION).size)
        assertEquals(2, file.descendants(CpElements.PARAMETER).size)
        assertEquals(4, file.descendants(CpElements.TYPE_REFERENCE).size)
        assertEquals(1, file.descendants(CpElements.DECLARATION_STATEMENT).size)
        assertTrue(file.descendants(CpElements.BINARY_EXPRESSION).isNotEmpty())
    }

    fun testValidParserFixturesBuildNonFlatPsi() {
        val repoRoot = repoRoot()
        val cases = sequenceOf(
            repoRoot.resolve("design/examples"),
            repoRoot.resolve("test/parser/cases/valid"),
        ).flatMap { root ->
            Files.walk(root).use { stream ->
                stream
                    .filter { it.isRegularFile() && it.toString().endsWith(".cp") }
                    .toList()
                    .asSequence()
            }
        }.toList()

        assertTrue("fixture list should not be empty", cases.isNotEmpty())
        for (path in cases) {
            val file = parse(Files.readString(path))
            val structuredNodes = file.descendants(CpElements.FUNCTION).size +
                file.descendants(CpElements.MODULE_HEADER).size +
                file.descendants(CpElements.IMPORT_DECLARATION).size
            assertTrue("$path should produce structured PSI", structuredNodes > 0)
        }
    }

    fun testValidParserFixturesDoNotProducePsiErrors() {
        for (path in validParserCases()) {
            val file = parse(Files.readString(path))
            val errors = file.collectPsiErrors()
            assertTrue("$path should not produce PSI errors: $errors", errors.isEmpty())
        }
    }

    fun testInvalidStatementRecoveryKeepsLaterDeclarations() {
        val file = parse(
            """
            main()
            {
                let before = 1;
                module;
                let after = 2;
            }
            """.trimIndent(),
        )

        val declarations = file.descendants(CpElements.LOCAL_DECLARATION).map { it.text }
        assertTrue("before declaration should survive recovery", "before" in declarations)
        assertTrue("after declaration should survive recovery", "after" in declarations)
    }

    fun testLambdaUsesFMarker() {
        val file = parse(
            """
            main() -> i32
            {
                let inc: f(i32) -> i32 = f(value) => value + 1;
                let add_bias = f(value: i32) -> i32 {
                    value + 1
                };
                return inc(1) + add_bias(2);
            }
            """.trimIndent(),
        )

        assertEquals(2, file.descendants(CpElements.LAMBDA_EXPRESSION).size)
        assertTrue(file.collectPsiErrors().isEmpty())
    }

    fun testIncompleteConceptDoesNotTrapParserRecovery() {
        val file = parse(
            """
            import math;

            add1<T>(x: T) -> T {
                return x + 1;
            }

            concept

            sum<T...>(args: T...) -> _
            {
                let sum = 0 as i64;
                return sum;
            }
            """.trimIndent(),
        )

        assertTrue(file.descendants(CpElements.CONCEPT_DECLARATION).isNotEmpty())
        assertTrue(file.collectPsiErrors().isNotEmpty())
    }

    private fun parse(text: String): PsiElement =
        myFixture.configureByText(CpFileType.INSTANCE, text)

    private fun validParserCases(): List<Path> {
        val repoRoot = repoRoot()
        return sequenceOf(
            repoRoot.resolve("design/examples"),
            repoRoot.resolve("test/parser/cases/valid"),
        ).flatMap { root ->
            Files.walk(root).use { stream ->
                stream
                    .filter { it.isRegularFile() && it.toString().endsWith(".cp") }
                    .toList()
                    .asSequence()
            }
        }.toList()
    }

    private fun PsiElement.collectPsiErrors(): List<String> {
        val result = mutableListOf<String>()
        collectPsiErrors(result)
        return result
    }

    private fun PsiElement.collectPsiErrors(result: MutableList<String>) {
        if (this is PsiErrorElement) {
            result += "$errorDescription at $textRange"
        }
        for (child in children) {
            child.collectPsiErrors(result)
        }
    }

    private fun repoRoot(): Path =
        Path.of(System.getProperty("cp.repo.root") ?: error("cp.repo.root is not configured"))
}
