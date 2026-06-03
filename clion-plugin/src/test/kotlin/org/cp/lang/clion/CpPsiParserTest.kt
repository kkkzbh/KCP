package org.cp.lang.clion

import com.intellij.lang.LanguageParserDefinitions
import com.intellij.testFramework.fixtures.BasePlatformTestCase
import com.intellij.psi.PsiElement
import com.intellij.psi.PsiErrorElement
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
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

    fun testExtraSemicolonDoesNotProducePsiError() {
        val file = parse(
            """
            main() -> i32
            {
                let value = 1;;
                return value;
            }
            """.trimIndent(),
        )

        assertTrue(file.collectPsiErrors().isEmpty())
    }

    fun testStaticDeclarationModifierDoesNotStealContextualName() {
        val file = parse(
            """
            main() -> i32
            {
                const static answer: i32 = 42;
                let static counter = 0;
                let static = 1;
                return answer + counter + static;
            }
            """.trimIndent(),
        )

        val declarations = file.descendants(CpElements.LOCAL_DECLARATION).map { it.text }
        assertEquals(listOf("answer", "counter", "static"), declarations)
        assertTrue(file.collectPsiErrors().isEmpty())
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

    fun testSelfReceiverParametersDoNotRequireColon() {
        val file = parse(
            """
            struct result {
                value: i32;
            }

            impl result {
                is_ok(self const&) -> bool
                {
                    return true;
                }

                set(self&, value: i32)
                {
                    self.value = value;
                }
            }
            """.trimIndent(),
        )

        assertEquals(3, file.descendants(CpElements.PARAMETER).size)
        assertTrue(file.collectPsiErrors().isEmpty())
    }

    fun testInferredFunctionParametersDoNotRequireColon() {
        val file = parse(
            """
            read(value const&) -> i32
            {
                return value;
            }

            borrow(value&) -> i32
            {
                return value;
            }

            take(value move&) -> i32
            {
                return value;
            }

            relay<T>(value: T forward&) -> T
            {
                return forward value;
            }

            read(value: T forward&)
            {
                consume(const value);
            }

            add(left, right) -> i32
            {
                return left + right;
            }
            """.trimIndent(),
        )

        assertEquals(7, file.descendants(CpElements.PARAMETER).size)
        assertTrue(file.collectPsiErrors().isEmpty())
    }

    fun testMixedInferredParameterSuffixAndTypeReportsPsiError() {
        val file = parse("bad(value&: i32) -> i32 { return value; }")

        assertTrue(file.collectPsiErrors().any { "inferred parameter suffix cannot be combined with an explicit type" in it })
    }

    fun testTupleIndexMemberExpressionParses() {
        val file = parse(
            """
            main() -> i32
            {
                let pair = (1, 2);
                return pair.0;
            }
            """.trimIndent(),
        )

        assertTrue(file.descendants(CpElements.MEMBER_NAME).any { it.text == "0" })
        assertTrue(file.collectPsiErrors().isEmpty())
    }

    fun testOperatorOverloadsBuildStructuredPsi() {
        val file = parse(
            """
            export operator +(left: vec2 const&, right: vec2 const&) -> vec2
            {
                return vec2{ left.x + right.x, left.y + right.y };
            }

            impl vec2 {
                operator [](self&, index: i32) -> i32&
                {
                    return x;
                }

                operator +=(self&, rhs: this const&)
                {
                    x += rhs.x;
                    y += rhs.y;
                }

                operator prefix ++(self&) -> this&
                {
                    return self;
                }

                operator postfix --(self&) -> this
                {
                    return self;
                }
            }
            """.trimIndent(),
        )

        val names = file.descendants(CpElements.FUNCTION_NAME).map { it.text }
        assertTrue("top-level operator should be parsed as a function", "operator +" in names)
        assertTrue("operator [] should parse both bracket tokens as the function name", "operator []" in names)
        assertTrue("operator += should be parsed as an impl function", "operator +=" in names)
        assertTrue("operator prefix ++ should include the affix in the function name", "operator prefix ++" in names)
        assertTrue("operator postfix -- should include the affix in the function name", "operator postfix --" in names)
        assertEquals(5, file.descendants(CpElements.FUNCTION).size)
        assertTrue(file.collectPsiErrors().isEmpty())
    }

    fun testBareIncrementOperatorDeclarationReportsPsiError() {
        val file = parse(
            """
            impl counter {
                operator ++(self&) -> this&
                {
                    return self;
                }
            }
            """.trimIndent(),
        )

        assertTrue(file.collectPsiErrors().any { "expected 'prefix' or 'postfix' before '++' or '--'" in it })
    }

    fun testDefaultCompareAndEnumSpaceshipBuildStructuredPsi() {
        val file = parse(
            """
            import std;

            enum marker : u8 {
                alpha = 1;
                beta = 2;
            }

            struct key {
                group: i32;
                marker: marker;
            }

            impl key {
                operator <=>(self const&, rhs: this const&) -> weak_ordering = default;
            }

            main()
            {
                let enum_relation = marker::alpha <=> marker::beta;
                let key_relation = key{ .group = 1, .marker = marker::alpha } <=> key{ .group = 1, .marker = marker::beta };
            }
            """.trimIndent(),
        )

        assertEquals(1, file.descendants(CpElements.ENUM_DECLARATION).size)
        assertEquals(1, file.descendants(CpElements.STRUCT_DECLARATION).size)
        assertTrue(file.descendants(CpElements.FUNCTION_NAME).any { it.text == "operator <=>" })
        assertTrue(file.descendants(CpElements.BINARY_EXPRESSION).count { it.text.contains("<=>") } >= 2)
        assertTrue(file.collectPsiErrors().isEmpty())
    }

    fun testRepeatedGenericBoundTargetsParse() {
        val file = parse(
            """
            struct iota_iter<T: equality_comparable<T> and T: incrementable> {
                current: T;
                end: T;
            }

            iota<T: equality_comparable<T> and T: incrementable>(begin: T, end: T) -> iota_iter<T>
            {
                return iota_iter<T>{ .current = begin, .end = end };
            }
            """.trimIndent(),
        )

        assertEquals(2, file.descendants(CpElements.GENERIC_PARAMETER).size)
        assertTrue(file.collectPsiErrors().isEmpty())
    }

    fun testCurrentCompilerSurfaceBuildsStructuredPsi() {
        val file = parse(
            """
            export type handle = opaque i32;

            enum status: i32 {
                ok = 0;
                fail = 1;
            }

            extern "C" puts(value: i8 const*) -> i32;

            struct callable {
                value: i32;
            }

            impl callable {
                operator ()(self const&, value: i32) -> i32
                {
                    return self.value + value;
                }
            }

            main() -> i32
            {
                let make = f<T>(value: T) -> T => value;
                let never: ! = fail();
                let owned = new callable{ .value = make<i32>(1) };
                let marker = nullptr;
                return status::ok as i32;
            }
            """.trimIndent(),
        )

        assertEquals(1, file.descendants(CpElements.TYPE_ALIAS).size)
        assertEquals(1, file.descendants(CpElements.ENUM_DECLARATION).size)
        assertEquals(2, file.descendants(CpElements.ENUM_CASE).size)
        assertTrue(file.descendants(CpElements.LAMBDA_EXPRESSION).isNotEmpty())
        assertTrue(file.descendants(CpElements.TYPE_REFERENCE).any { it.text == "!" })
        assertTrue(file.collectPsiErrors().isEmpty())
    }

    fun testTemplateIfTemplateForRefAndStorageSyntaxBuildStructuredPsi() {
        val file = parse(
            """
            struct box<T, N> {
                items: storage [T; N];
                single: storage T;
                count: usize = 0 as usize;
            }

            select<T>(value: T const&) -> i32
            {
                template if(read_type<decltype(value)> == i32) {
                    return 1;
                } else template if(read_type<decltype(value)> == bool) {
                    return 2;
                } else {
                    return 3;
                }
            }

            collect<T...>(values: T...) -> i32
            {
                let total = 0;
                template for(const ref value : values...) {
                    total += select(value);
                }
                let fixed = storage [i32; 4]{};
                let one = storage i32{};
                return total;
            }
            """.trimIndent(),
        )

        assertEquals(1, file.descendants(CpElements.TEMPLATE_IF_STATEMENT).size)
        assertEquals(1, file.descendants(CpElements.TEMPLATE_FOR_STATEMENT).size)
        assertTrue(file.descendants(CpElements.TYPE_REFERENCE).any { it.text == "storage [T; N]" })
        assertTrue(file.descendants(CpElements.STRUCT_INIT_EXPRESSION).any { it.text == "storage [i32; 4]{}" })
        assertTrue(file.descendants(CpElements.STRUCT_INIT_EXPRESSION).any { it.text == "storage i32{}" })
        assertTrue(file.collectPsiErrors().isEmpty())
    }

    fun testVariantMatchWithTemplatePackBuildsStructuredPsi() {
        val file = parse(
            """
            variant calc_result<T> {
                empty;
                total(T);
            }

            sum<T...>(values: T...) -> calc_result<i32>
            {
                let total = 0;
                template for(let value : values...) {
                    total = total + value;
                }
                return calc_result<i32>::total(total);
            }

            main() -> i32
            {
                return match sum(10, 20, 12) {
                    .total(value) => value,
                    .empty => 0,
                };
            }
            """.trimIndent(),
        )

        assertEquals(1, file.descendants(CpElements.VARIANT_DECLARATION).size)
        assertEquals(1, file.descendants(CpElements.TEMPLATE_FOR_STATEMENT).size)
        assertEquals(1, file.descendants(CpElements.MATCH_EXPRESSION).size)
        assertTrue(file.descendants(CpElements.GENERIC_PARAMETER).any { it.text == "T..." })
        assertTrue(file.collectPsiErrors().isEmpty())
    }

    fun testForwardTypePackAndInferredReturnBuildStructuredPsi() {
        val file = parse(
            """
            main() -> i32
            {
                return type_count<i32, bool, i32>() * 10 + first_nonzero(0, 12);
            }

            type_count<T...>()
            {
                let total = 0;
                template for(type U : T...) {
                    type current = U;
                    total = total + 1;
                }
                return total;
            }

            first_nonzero<T...>(values: T...)
            {
                template for(let value : values...) {
                    if(value != 0) {
                        return value;
                    }
                }
                return 0;
            }
            """.trimIndent(),
        )

        assertEquals(3, file.descendants(CpElements.FUNCTION).size)
        assertEquals(2, file.descendants(CpElements.TEMPLATE_FOR_STATEMENT).size)
        assertTrue(file.descendants(CpElements.GENERIC_PARAMETER).all { it.text == "T..." })
        assertTrue(file.collectPsiErrors().isEmpty())
    }

    fun testCallableTypePackReturnBuildsStructuredPsi() {
        val file = parse(
            """
            import std.meta;

            make<F, Args...>() -> call_result<F, Args...>
            requires F: callable<Args...>
            {
                return 42;
            }

            main() -> i32
            {
                return make<f(i32, bool) -> i32, i32, bool>();
            }
            """.trimIndent(),
        )

        assertTrue(file.descendants(CpElements.TYPE_ARGUMENT).any { it.text == "Args..." })
        assertTrue(file.descendants(CpElements.TYPE_REFERENCE).any { it.text == "call_result<F, Args...>" })
        assertTrue(file.collectPsiErrors().isEmpty())
    }

    fun testCallableTailTypePackReturnBuildsStructuredPsi() {
        val file = parse(
            """
            import std.meta;

            make<F, Args...>() -> call_result<F, Args..., bool>
            requires F: callable<Args..., bool>
            {
                return 42;
            }

            main() -> i32
            {
                return make<f(i32, bool) -> i32, i32>();
            }
            """.trimIndent(),
        )

        val typeArguments = file.descendants(CpElements.TYPE_ARGUMENT).map { it.text }
        assertTrue(typeArguments.any { it == "Args..." })
        assertTrue(typeArguments.any { it == "bool" })
        assertTrue(file.descendants(CpElements.TYPE_REFERENCE).any { it.text == "call_result<F, Args..., bool>" })
        assertTrue(file.collectPsiErrors().isEmpty())
    }

    fun testIncompleteVariantMatchTemplatePackArmDoesNotTrapRecovery() {
        val file = parse(
            """
            variant calc_result<T> {
                empty;
                total(T);
            }

            sum<T...>(values: T...) -> calc_result<i32>
            {
                let total = 0;
                return match calc_result<i32>::total(total) {
                    .total(value) => template for(let item : values...) {
                        total = total + item;
                    },
                    .empty => 0,
                };
            }
            """.trimIndent(),
        )

        assertEquals(1, file.descendants(CpElements.VARIANT_DECLARATION).size)
        assertEquals(1, file.descendants(CpElements.MATCH_EXPRESSION).size)
        assertTrue(file.collectPsiErrors().isNotEmpty())
    }

    fun testRealLabAndStdCompilerSyntaxDoNotFlattenPsi() {
        val repoRoot = repoRoot()
        val grammar = parse(Files.readString(repoRoot.resolve("lab/parser/grammar.cp")))
        assertTrue(grammar.descendants(CpElements.TEMPLATE_IF_STATEMENT).isNotEmpty())
        assertTrue(grammar.descendants(CpElements.TEMPLATE_FOR_STATEMENT).isNotEmpty())
        val grammarErrors = grammar.collectPsiErrors()
        assertTrue("lab/parser/grammar.cp should not produce PSI errors: $grammarErrors", grammarErrors.isEmpty())

        val btreeStorage = parse(Files.readString(repoRoot.resolve("std/collections/detail/btree_storage.cp")))
        assertTrue(btreeStorage.descendants(CpElements.TYPE_REFERENCE).any { it.text == "storage [K; 39]" })
        assertTrue(btreeStorage.descendants(CpElements.TYPE_REFERENCE).any { it.text == "storage [V; 39]" })
        val btreeStorageErrors = btreeStorage.collectPsiErrors()
        assertTrue(
            "std/collections/detail/btree_storage.cp should not produce PSI errors: $btreeStorageErrors",
            btreeStorageErrors.isEmpty(),
        )
    }

    fun testRealLabAndStdSourcesDoNotProducePsiErrors() {
        val repoRoot = repoRoot()
        val failures = sequenceOf(repoRoot.resolve("lab"), repoRoot.resolve("std"))
            .flatMap { root ->
                Files.walk(root).use { stream ->
                    stream
                        .filter { it.isRegularFile() && it.toString().endsWith(".cp") }
                        .toList()
                        .asSequence()
                }
            }
            .mapNotNull { path ->
                val file = parse(Files.readString(path))
                val errors = file.collectPsiErrors()
                if (errors.isEmpty()) {
                    null
                } else {
                    "${repoRoot.relativize(path)}: $errors"
                }
            }
            .toList()

        assertTrue("real lab/std sources should not produce PSI errors: $failures", failures.isEmpty())
    }

    fun testNestedGenericAssociatedConstructorParses() {
        val file = parse(
            """
            parse(tokens: vector<token>) -> optional<vector<op_symbol>>
            {
                let result = vector<op_symbol>{};
                return optional<vector<op_symbol>>::some(move result);
            }
            """.trimIndent(),
        )

        assertTrue(file.descendants(CpElements.ASSOCIATED_NAME_EXPRESSION).any { it.text == "optional<vector<op_symbol>>::some" })
        assertTrue(file.collectPsiErrors().isEmpty())
    }

    fun testRunMarkerOnlyAppearsOnTopLevelMain() {
        val file = parse(
            """
            main() -> i32
            {
                return 0;
            }

            helper() -> i32
            {
                return 1;
            }

            impl thing {
                main(self&) -> i32
                {
                    return 2;
                }
            }
            """.trimIndent(),
        )
        val contributor = CpRunLineMarkerContributor()
        val names = file.descendants(CpElements.FUNCTION_NAME)

        assertNotNull(contributor.getInfo(names.first { it.text == "main" }.firstChild))
        assertNull(contributor.getInfo(names.single { it.text == "helper" }.firstChild))
        assertNull(contributor.getInfo(names.last { it.text == "main" }.firstChild))
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
