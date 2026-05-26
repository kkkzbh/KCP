package org.cp.lang.clion

import com.intellij.extapi.psi.ASTWrapperPsiElement
import com.intellij.extapi.psi.PsiFileBase
import com.intellij.lang.ASTNode
import com.intellij.lang.ParserDefinition
import com.intellij.lang.PsiParser
import com.intellij.lexer.Lexer
import com.intellij.openapi.fileEditor.OpenFileDescriptor
import com.intellij.openapi.project.Project
import com.intellij.platform.backend.navigation.NavigationRequest
import com.intellij.pom.Navigatable
import com.intellij.psi.FileViewProvider
import com.intellij.psi.PsiElement
import com.intellij.psi.PsiFile
import com.intellij.psi.PsiFileFactory
import com.intellij.psi.PsiNameIdentifierOwner
import com.intellij.psi.PsiReference
import com.intellij.psi.TokenType
import com.intellij.psi.tree.IFileElementType
import com.intellij.psi.tree.IElementType
import com.intellij.psi.tree.TokenSet

class CpParserDefinition : ParserDefinition {
    override fun createLexer(project: Project?): Lexer = CpLexer()

    override fun createParser(project: Project?): PsiParser = CpPsiParser()

    override fun getFileNodeType(): IFileElementType = FILE

    override fun getCommentTokens(): TokenSet = CpTypes.COMMENT_TOKENS

    override fun getStringLiteralElements(): TokenSet = CpTypes.STRING_TOKENS

    override fun createElement(node: ASTNode): PsiElement =
        if (node.elementType in CpNamedElementTypes) {
            CpNamedElement(node)
        } else {
            CpPsiElement(node)
        }

    override fun createFile(viewProvider: FileViewProvider): PsiFile = CpFile(viewProvider)

    override fun getWhitespaceTokens(): TokenSet = TokenSet.create(TokenType.WHITE_SPACE)

    override fun spaceExistenceTypeBetweenTokens(left: ASTNode, right: ASTNode): ParserDefinition.SpaceRequirements =
        ParserDefinition.SpaceRequirements.MAY

    private companion object {
        private val FILE = IFileElementType(CpLanguage)
    }
}

class CpFile(viewProvider: FileViewProvider) : PsiFileBase(viewProvider, CpLanguage) {
    override fun getFileType() = CpFileType.INSTANCE

    override fun toString(): String = "cp 文件"
}

open class CpPsiElement(node: ASTNode) : ASTWrapperPsiElement(node), Navigatable {
    override fun getReference(): PsiReference? =
        if (cpElementType() in CpReferenceElementTypes) {
            CpReference(this)
        } else {
            null
        }

    override fun canNavigate(): Boolean =
        navigationDescriptor() != null

    override fun canNavigateToSource(): Boolean =
        canNavigate()

    override fun navigate(requestFocus: Boolean) {
        navigationDescriptor()?.navigate(requestFocus)
    }

    override fun navigationRequest(): NavigationRequest? =
        navigationDescriptor()?.navigationRequest()

    private fun navigationDescriptor(): OpenFileDescriptor? {
        val file = containingFile?.virtualFile ?: return null
        return OpenFileDescriptor(project, file, textOffset)
    }
}

class CpNamedElement(node: ASTNode) : CpPsiElement(node), PsiNameIdentifierOwner {
    override fun getNameIdentifier(): PsiElement? = firstChild ?: this

    override fun getName(): String = text

    override fun setName(name: String): PsiElement {
        val references = containingFile
            ?.let { file ->
                CpFileSymbolIndex.get(file).referenceElements
                    .mapNotNull { it.reference }
                    .filter { it.isReferenceTo(this) }
                    .map { it.element }
                    .filter { it != this }
                    .toList()
            }
            .orEmpty()
        for (reference in references) {
            if (reference is CpNamedElement && reference.isValid) {
                reference.renameTo(name)
            }
        }
        return renameTo(name)
    }

    fun renameTo(name: String): PsiElement {
        val replacement = replacementElement(cpElementType(), name)
            ?: return this
        return replace(replacement)
    }

    private fun replacementElement(type: IElementType?, name: String): PsiElement? {
        val file = PsiFileFactory.getInstance(project).createFileFromText(
            "rename.cp",
            CpFileType.INSTANCE,
            replacementSource(type, name),
        )
        return file.descendants(type ?: return null).firstOrNull { it.text == name }
    }

    private fun replacementSource(type: IElementType?, name: String): String =
        when (type) {
            CpElements.MODULE_NAME -> "export module $name;"
            CpElements.IMPORT_NAME -> "import $name;"
            CpElements.FUNCTION_NAME -> "$name() {}"
            CpElements.PARAMETER_NAME -> "main($name: i32) {}"
            CpElements.LOCAL_DECLARATION -> "main() { let $name = 0; }"
            CpElements.MATCH_BINDING -> "main() { return match value { .some($name) => $name, }; }"
            CpElements.TYPE_NAME -> "struct $name {}"
            CpElements.GENERIC_PARAMETER_NAME -> "main<$name>() {}"
            CpElements.FIELD_DECLARATION -> "struct item { $name: i32; }"
            CpElements.ENUM_CASE_NAME -> "enum value: i32 { $name = 0; }"
            CpElements.VARIANT_CASE_NAME -> "variant value { $name; }"
            CpElements.MEMBER_NAME -> "main() { return value.$name; }"
            CpElements.ASSOCIATED_NAME -> "main() { return value::$name; }"
            CpElements.LOOP_LABEL -> "main() { for:$name(let item: values) { break $name; } }"
            CpElements.NAME_EXPRESSION -> "main() { return $name; }"
            else -> "main() { let $name = 0; }"
        }
}

private val CpNamedElementTypes = setOf(
    CpElements.MODULE_NAME,
    CpElements.IMPORT_NAME,
    CpElements.FUNCTION_NAME,
    CpElements.PARAMETER_NAME,
    CpElements.LOCAL_DECLARATION,
    CpElements.MATCH_BINDING,
    CpElements.TYPE_NAME,
    CpElements.GENERIC_PARAMETER_NAME,
    CpElements.FIELD_DECLARATION,
    CpElements.ENUM_CASE_NAME,
    CpElements.VARIANT_CASE_NAME,
    CpElements.MEMBER_NAME,
    CpElements.ASSOCIATED_NAME,
    CpElements.LOOP_LABEL,
    CpElements.NAME_EXPRESSION,
)

private val CpReferenceElementTypes = setOf(
    CpElements.NAME_EXPRESSION,
    CpElements.TYPE_NAME,
    CpElements.IMPORT_NAME,
    CpElements.MEMBER_NAME,
    CpElements.ASSOCIATED_NAME,
    CpElements.LOOP_LABEL,
    CpElements.VARIANT_CASE_NAME,
    CpElements.FIELD_DECLARATION,
)
