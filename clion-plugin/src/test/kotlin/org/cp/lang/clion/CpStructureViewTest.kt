package org.cp.lang.clion

import com.intellij.lang.LanguageParserDefinitions
import com.intellij.testFramework.fixtures.BasePlatformTestCase
import org.junit.Assert.assertFalse

class CpStructureViewTest : BasePlatformTestCase() {
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

    fun testStructureIncludesTypesMembersConceptsAndImplItems() {
        myFixture.configureByText(
            CpFileType.INSTANCE,
            """
            export module demo;
            import std.io;

            struct box {
                value: i32;
            }

            enum state: i32 {
                ok = 0;
            }

            variant option {
                some(i32);
                none;
            }

            concept readable<T> {
                type item;
                requires HasRead<T>;
                read(self const&) -> item;
            }

            impl box {
                type item = i32;
                get(self const&) -> i32
                {
                    return self.value;
                }
            }

            main() {}
            """.trimIndent(),
        )

        val labels = CpStructureEngine.labels(myFixture.file)

        assertContainsElements(
            labels,
            "module demo",
            "import std.io",
            "struct box",
            "value: i32",
            "enum state",
            "ok",
            "variant option",
            "some(i32)",
            "none",
            "concept readable",
            "type item",
            "requires HasRead<T>;",
            "read",
            "impl box",
            "get",
            "main",
        )
    }

    fun testStructureCacheInvalidatesAfterEdit() {
        myFixture.configureByText(
            CpFileType.INSTANCE,
            """
            main() {}
            <caret>
            """.trimIndent(),
        )

        val first = CpStructureEngine.labels(myFixture.file)
        myFixture.type("helper() {}\n")
        val second = CpStructureEngine.labels(myFixture.file)

        assertFalse("helper" in first)
        assertContainsElements(second, "main", "helper")
    }
}
