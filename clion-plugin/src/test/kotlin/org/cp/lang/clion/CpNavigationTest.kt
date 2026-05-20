package org.cp.lang.clion

import com.intellij.lang.LanguageParserDefinitions
import com.intellij.psi.PsiElement
import com.intellij.testFramework.fixtures.BasePlatformTestCase
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull

class CpNavigationTest : BasePlatformTestCase() {
    private val parserDefinition = CpParserDefinition()
    private val handler = CpGotoDeclarationHandler()

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

    fun testGotoLocalDeclaration() {
        myFixture.configureByText(
            CpFileType.INSTANCE,
            """
            main() -> i32
            {
                let sum: i32 = 1;
                return <caret>sum;
            }
            """.trimIndent(),
        )

        val target = singleTarget()

        assertEquals(CpElements.LOCAL_DECLARATION, target.cpElementType())
        assertEquals("sum", target.text)
    }

    fun testGotoParameterDeclaration() {
        myFixture.configureByText(
            CpFileType.INSTANCE,
            """
            add(value: i32) -> i32
            {
                return <caret>value;
            }
            """.trimIndent(),
        )

        val target = singleTarget()

        assertEquals(CpElements.PARAMETER_NAME, target.cpElementType())
        assertEquals("value", target.text)
    }

    fun testGotoImportedFunctionDeclaration() {
        myFixture.addFileToProject(
            "math.cp",
            """
            export module math;

            export add(left: i32, right: i32) -> i32
            {
                return left + right;
            }
            """.trimIndent(),
        )
        myFixture.configureByText(
            CpFileType.INSTANCE,
            """
            import math;

            main() -> i32
            {
                return <caret>add(1, 2);
            }
            """.trimIndent(),
        )

        val target = singleTarget()

        assertEquals(CpElements.FUNCTION_NAME, target.cpElementType())
        assertEquals("add", target.text)
        assertEquals("math.cp", target.containingFile.name)
    }

    fun testGotoImportModuleDeclaration() {
        myFixture.addFileToProject(
            "math.cp",
            """
            export module math;

            export add(left: i32, right: i32) -> i32
            {
                return left + right;
            }
            """.trimIndent(),
        )
        myFixture.configureByText(
            CpFileType.INSTANCE,
            """
            import <caret>math;
            """.trimIndent(),
        )

        val target = singleTarget()

        assertEquals(CpElements.MODULE_NAME, target.cpElementType())
        assertEquals("math", target.text)
        assertEquals("math.cp", target.containingFile.name)
    }

    fun testGotoTypeAndFieldDeclarations() {
        myFixture.configureByText(
            CpFileType.INSTANCE,
            """
            struct item {
                value: i32;
            }

            main() -> i32
            {
                let current: <caret>item = item{ .value = 1 };
                return current.value;
            }
            """.trimIndent(),
        )

        val typeTarget = singleTarget()

        assertEquals(CpElements.TYPE_NAME, typeTarget.cpElementType())
        assertEquals("item", typeTarget.text)

        val member = myFixture.file.descendants(CpElements.MEMBER_NAME).single { it.text == "value" }
        val fieldTarget = singleTarget(member.firstChild ?: member)

        assertEquals(CpElements.FIELD_DECLARATION, fieldTarget.cpElementType())
        assertEquals("value", fieldTarget.text)
    }

    fun testGotoMemberFunctionDeclarationUsesSemanticTarget() {
        myFixture.configureByText(
            CpFileType.INSTANCE,
            """
            struct box {
                value: i32;
            }

            impl box {
                get(self const&) -> i32
                {
                    return self.value;
                }
            }

            main() -> i32
            {
                let item = box{ .value = 1 };
                return item.<caret>get();
            }
            """.trimIndent(),
        )

        val target = singleTarget()

        assertEquals(CpElements.FUNCTION_NAME, target.cpElementType())
        assertEquals("get", target.text)
    }

    private fun singleTarget(element: PsiElement = caretElement()): PsiElement {
        val targets = handler.getGotoDeclarationTargets(element, myFixture.caretOffset, myFixture.editor)
        assertNotNull("source=${element.text}:${element.cpElementType()} parent=${element.parent?.text}:${element.parent?.cpElementType()}", targets)
        assertEquals(1, targets!!.size)
        return targets.single()
    }

    private fun caretElement(): PsiElement =
        myFixture.file.findElementAt(myFixture.caretOffset) ?: error("caret element is missing")
}
