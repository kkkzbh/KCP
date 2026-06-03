package org.cp.lang.clion

import com.intellij.lang.LanguageParserDefinitions
import com.intellij.testFramework.fixtures.BasePlatformTestCase
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue

class CpDocumentationTest : BasePlatformTestCase() {
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

    fun testFunctionDocumentationShowsSignatureAndRequires() {
        myFixture.configureByText(
            CpFileType.INSTANCE,
            """
            add<T>(left: T, right: T) -> T requires Add<T>
            {
                return left + right;
            }
            """.trimIndent(),
        )

        val function = myFixture.file.descendants(CpElements.FUNCTION).single()
        val doc = CpDocumentationEngine.documentation(function)

        assertNotNull(doc)
        assertTrue(doc!!, "add&lt;T&gt;(left: T, right: T) -&gt; T" in doc)
        assertTrue(doc, "requires Add&lt;T&gt;" in doc)
    }

    fun testStructDocumentationShowsFields() {
        myFixture.configureByText(
            CpFileType.INSTANCE,
            """
            struct box {
                value: i32;
                ready: bool;
            }
            """.trimIndent(),
        )

        val typeName = myFixture.file.descendants(CpElements.TYPE_NAME).first { it.text == "box" }
        val doc = CpDocumentationEngine.documentation(typeName)

        assertNotNull(doc)
        assertTrue(doc!!, "struct box" in doc)
        assertTrue(doc, "value: i32" in doc)
        assertTrue(doc, "ready: bool" in doc)
    }

    fun testDocumentationCacheInvalidatesAfterEdit() {
        myFixture.configureByText(
            CpFileType.INSTANCE,
            """
            struct box {
                value: i32;
                <caret>
            }
            """.trimIndent(),
        )

        val firstTypeName = myFixture.file.descendants(CpElements.TYPE_NAME).first { it.text == "box" }
        val first = CpDocumentationEngine.documentation(firstTypeName)
        myFixture.type("ready: bool;\n")
        val secondTypeName = myFixture.file.descendants(CpElements.TYPE_NAME).first { it.text == "box" }
        val second = CpDocumentationEngine.documentation(secondTypeName)

        assertNotNull(first)
        assertNotNull(second)
        assertFalse(first!!, "ready: bool" in first)
        assertTrue(second!!, "ready: bool" in second)
    }

    fun testConceptDocumentationShowsRequirements() {
        myFixture.configureByText(
            CpFileType.INSTANCE,
            """
            concept readable<T> {
                type item;
                requires HasBegin<T>;
                next(self const&) -> item;
            }
            """.trimIndent(),
        )

        val concept = myFixture.file.descendants(CpElements.CONCEPT_DECLARATION).single()
        val doc = CpDocumentationEngine.documentation(concept.directChild(CpElements.TYPE_NAME)!!)

        assertNotNull(doc)
        assertTrue(doc!!, "concept readable" in doc)
        assertTrue(doc, "item" in doc)
        assertTrue(doc, "next(self const&amp;) -&gt; item" in doc)
        assertTrue(doc, "requires HasBegin&lt;T&gt;" in doc)
    }

    fun testDocumentationProviderDelegatesToEngineAndKeepsCpContextElement() {
        myFixture.configureByText(
            CpFileType.INSTANCE,
            """
            struct box {
                value: i32;
            }
            """.trimIndent(),
        )

        val provider = CpDocumentationProvider()
        val typeName = myFixture.file.descendants(CpElements.TYPE_NAME).first { it.text == "box" }

        assertTrue(provider.getQuickNavigateInfo(typeName, null)!!.contains("struct box"))
        assertTrue(provider.generateDoc(typeName, null)!!.contains("value: i32"))
        assertNull(provider.generateDoc(myFixture.file, null))
        assertNotNull(provider.getCustomDocumentationElement(myFixture.editor, myFixture.file, typeName))
    }
}
