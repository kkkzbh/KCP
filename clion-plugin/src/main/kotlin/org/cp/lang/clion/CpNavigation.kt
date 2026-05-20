package org.cp.lang.clion

import com.intellij.codeInsight.navigation.actions.GotoDeclarationHandler
import com.intellij.openapi.actionSystem.DataContext
import com.intellij.openapi.editor.Editor
import com.intellij.openapi.vfs.LocalFileSystem
import com.intellij.psi.PsiElement
import com.intellij.psi.PsiFile
import com.intellij.psi.PsiManager
import com.intellij.psi.search.FileTypeIndex
import com.intellij.psi.search.GlobalSearchScope
import com.intellij.psi.tree.IElementType
import java.nio.file.Path

class CpGotoDeclarationHandler : GotoDeclarationHandler {
    override fun getGotoDeclarationTargets(sourceElement: PsiElement?, offset: Int, editor: Editor?): Array<PsiElement>? {
        val element = sourceElement?.cpNavigationElement() ?: return null
        if (element.containingFile?.language != CpLanguage) {
            return null
        }

        val target = CpSemanticDeclarationResolver.resolve(element, editor)
            ?: CpDeclarationResolver.resolve(element)
            ?: return null
        return arrayOf(target)
    }

    override fun getActionText(context: DataContext): String? = null
}

object CpSemanticDeclarationResolver {
    fun resolve(element: PsiElement, editor: Editor?): PsiElement? {
        val file = element.containingFile ?: return null
        val activeText = editor?.document?.text ?: file.text
        val request = CpProjectSnapshotCollector.collect(file, activeText) ?: return null
        val result = CpHelperRunner.inspect(request)
        if (result.navigation.isEmpty()) {
            return null
        }

        val range = element.textRange
        val target = result.navigation
            .filter { navigation ->
                navigation.sourceStartOffset >= range.startOffset &&
                    navigation.sourceEndOffset <= range.endOffset
            }
            .minByOrNull { it.sourceEndOffset - it.sourceStartOffset }
            ?: return null

        val targetFile = targetFile(file, request, target.targetFile) ?: return null
        return targetFile.findElementAt(target.targetStartOffset)?.cpNavigationTargetElement()
    }

    private fun targetFile(activeFile: PsiFile, request: CpInspectionRequest, path: String): PsiFile? {
        val normalized = normalizePath(path)
        if (normalizePath(activeFile.virtualFile?.path ?: activeFile.name) == normalized) {
            return activeFile
        }

        val psiManager = PsiManager.getInstance(activeFile.project)
        val projectFile = FileTypeIndex.getFiles(CpFileType.INSTANCE, GlobalSearchScope.projectScope(activeFile.project))
            .firstOrNull { normalizePath(it.path) == normalized }
        if (projectFile != null) {
            return psiManager.findFile(projectFile)
        }

        if (request.files.none { normalizePath(it.path) == normalized }) {
            return null
        }
        return LocalFileSystem.getInstance().refreshAndFindFileByPath(path)?.let { psiManager.findFile(it) }
    }
}

object CpDeclarationResolver {
    val referenceTypes = setOf(
        CpElements.NAME_EXPRESSION,
        CpElements.TYPE_NAME,
        CpElements.IMPORT_NAME,
        CpElements.MEMBER_NAME,
        CpElements.ASSOCIATED_NAME,
        CpElements.LOOP_LABEL,
        CpElements.VARIANT_CASE_NAME,
        CpElements.FIELD_DECLARATION,
    )

    val declarationTypes = setOf(
        CpElements.MODULE_NAME,
        CpElements.FUNCTION_NAME,
        CpElements.PARAMETER_NAME,
        CpElements.LOCAL_DECLARATION,
        CpElements.MATCH_BINDING,
        CpElements.TYPE_NAME,
        CpElements.GENERIC_PARAMETER_NAME,
        CpElements.FIELD_DECLARATION,
        CpElements.ENUM_CASE_NAME,
        CpElements.VARIANT_CASE_NAME,
        CpElements.LOOP_LABEL,
    )

    fun resolve(element: PsiElement): PsiElement? =
        when (element.cpElementType()) {
            CpElements.NAME_EXPRESSION -> resolveNameExpression(element)
            CpElements.TYPE_NAME -> resolveTypeName(element)
            CpElements.IMPORT_NAME -> resolveModuleName(element)
            CpElements.MEMBER_NAME -> resolveMemberName(element)
            CpElements.ASSOCIATED_NAME -> resolveAssociatedName(element)
            CpElements.LOOP_LABEL -> resolveLoopLabel(element)
            CpElements.VARIANT_CASE_NAME -> resolveVariantCase(element)
            CpElements.FIELD_DECLARATION -> resolveFieldInitializer(element)
            else -> null
        }?.takeUnless { it == element }

    private fun resolveNameExpression(element: PsiElement): PsiElement? =
        resolveLocalValue(element)
            ?: resolveFunction(element)
            ?: resolveVariantOrEnumCase(element)

    private fun resolveLocalValue(element: PsiElement): PsiElement? {
        val name = element.text
        val offset = element.textRange.startOffset
        val candidates = mutableListOf<PsiElement>()
        var current: PsiElement? = element.parent
        while (current != null && current !is PsiFile) {
            when (current.cpElementType()) {
                CpElements.FUNCTION,
                CpElements.LAMBDA_EXPRESSION,
                CpElements.BLOCK,
                CpElements.BLOCK_EXPRESSION,
                CpElements.MATCH_ARM -> {
                    candidates += current.descendants(CpElements.PARAMETER_NAME)
                    candidates += current.descendants(CpElements.LOCAL_DECLARATION)
                    candidates += current.descendants(CpElements.MATCH_BINDING)
                }
            }
            current = current.parent
        }
        return candidates
            .asSequence()
            .filter { it.text == name && it.textRange.startOffset < offset }
            .maxByOrNull { it.textRange.startOffset }
    }

    private fun resolveFunction(element: PsiElement): PsiElement? {
        val name = element.text
        return element.navigationFiles()
            .asSequence()
            .flatMap { it.descendants(CpElements.FUNCTION_NAME).asSequence() }
            .firstOrNull { it.text == name }
    }

    private fun resolveTypeName(element: PsiElement): PsiElement? {
        if (element.isTypeDeclarationName()) {
            return null
        }
        val name = element.text
        return resolveLocalType(element, name)
            ?: element.navigationFiles()
                .asSequence()
                .flatMap { it.descendants(CpElements.TYPE_NAME).asSequence() }
                .firstOrNull { it.text == name && it.isTypeDeclarationName() }
    }

    private fun resolveLocalType(element: PsiElement, name: String): PsiElement? {
        val offset = element.textRange.startOffset
        val candidates = mutableListOf<PsiElement>()
        var current: PsiElement? = element.parent
        while (current != null && current !is PsiFile) {
            candidates += current.descendants(CpElements.GENERIC_PARAMETER_NAME)
            candidates += current.descendants(CpElements.TYPE_NAME)
                .filter { it.textRange.startOffset < offset && it.isLocalTypeAliasName() }
            current = current.parent
        }
        return candidates
            .asSequence()
            .filter { it.text == name && it.textRange.startOffset < offset }
            .maxByOrNull { it.textRange.startOffset }
    }

    private fun resolveModuleName(element: PsiElement): PsiElement? {
        val name = element.text
        return element.navigationFiles()
            .asSequence()
            .flatMap { it.descendants(CpElements.MODULE_NAME).asSequence() }
            .firstOrNull { it.text == name }
    }

    private fun resolveMemberName(element: PsiElement): PsiElement? {
        val name = element.text
        return element.navigationFiles()
            .asSequence()
            .flatMap { it.descendants(CpElements.FIELD_DECLARATION).asSequence() }
            .firstOrNull { it.text == name && it.parent?.cpElementType() == CpElements.STRUCT_FIELD }
            ?: resolveFunction(element)
    }

    private fun resolveAssociatedName(element: PsiElement): PsiElement? =
        resolveVariantOrEnumCase(element)
            ?: resolveFunction(element)
            ?: resolveTypeName(element)

    private fun resolveLoopLabel(element: PsiElement): PsiElement? {
        if (element.parent?.cpElementType() != CpElements.BREAK_STATEMENT &&
            element.parent?.cpElementType() != CpElements.CONTINUE_STATEMENT
        ) {
            return null
        }
        val name = element.text
        val offset = element.textRange.startOffset
        return element.containingFile
            .descendants(CpElements.LOOP_LABEL)
            .asSequence()
            .filter { it.text == name && it.textRange.startOffset < offset }
            .filter { it.parent?.cpElementType() == CpElements.FOR_STATEMENT }
            .maxByOrNull { it.textRange.startOffset }
    }

    private fun resolveVariantCase(element: PsiElement): PsiElement? {
        if (element.parent?.cpElementType() != CpElements.MATCH_PATTERN) {
            return null
        }
        return resolveVariantOrEnumCase(element)
    }

    private fun resolveFieldInitializer(element: PsiElement): PsiElement? {
        if (element.parent?.cpElementType() != CpElements.STRUCT_INITIALIZER) {
            return null
        }
        return resolveMemberName(element)
    }

    private fun resolveVariantOrEnumCase(element: PsiElement): PsiElement? {
        val name = element.text
        return element.navigationFiles()
            .asSequence()
            .flatMap { file ->
                sequenceOf(
                    file.descendants(CpElements.VARIANT_CASE_NAME).asSequence()
                        .filter { it.parent?.cpElementType() == CpElements.VARIANT_CASE },
                    file.descendants(CpElements.ENUM_CASE_NAME).asSequence(),
                ).flatten()
            }
            .firstOrNull { it.text == name }
    }

    private fun PsiElement.navigationFiles(): List<PsiFile> {
        val file = containingFile ?: return emptyList()
        val request = CpProjectSnapshotCollector.collect(file, file.text)
        val wantedPaths = request?.files?.map { normalizePath(it.path) } ?: listOf(normalizePath(file.virtualFile?.path ?: file.name))
        val psiManager = PsiManager.getInstance(project)
        val projectFiles = FileTypeIndex.getFiles(CpFileType.INSTANCE, GlobalSearchScope.projectScope(project))
            .associateBy { normalizePath(it.path) }
        val files = linkedMapOf<String, PsiFile>()

        file.virtualFile?.path?.let { files[normalizePath(it)] = file }
        for (path in wantedPaths) {
            val psiFile = when {
                files.containsKey(path) -> files[path]
                projectFiles.containsKey(path) -> psiManager.findFile(projectFiles[path]!!)
                else -> LocalFileSystem.getInstance().refreshAndFindFileByPath(path)?.let { psiManager.findFile(it) }
            } ?: continue
            files[path] = psiFile
        }
        return files.values.toList()
    }

    private fun PsiElement.isTypeDeclarationName(): Boolean =
        when (parent?.cpElementType()) {
            CpElements.STRUCT_DECLARATION,
            CpElements.ENUM_DECLARATION,
            CpElements.VARIANT_DECLARATION,
            CpElements.CONCEPT_DECLARATION -> true
            CpElements.TYPE_ALIAS -> parent?.firstDescendant(CpElements.TYPE_NAME) == this
            else -> false
        }

    private fun PsiElement.isLocalTypeAliasName(): Boolean =
        parent?.cpElementType() == CpElements.TYPE_ALIAS_STATEMENT &&
            parent?.firstDescendant(CpElements.TYPE_NAME) == this

}

private fun PsiElement.cpNavigationElement(): PsiElement? {
    var current: PsiElement? = this
    while (current != null) {
        if (current.cpElementType() in CpDeclarationResolver.referenceTypes) {
            return current
        }
        current = current.parent
    }
    return null
}

private fun PsiElement.cpNavigationTargetElement(): PsiElement? {
    var current: PsiElement? = this
    while (current != null) {
        if (current.cpElementType() in CpDeclarationResolver.declarationTypes) {
            return current
        }
        current = current.parent
    }
    return null
}

private fun normalizePath(path: String): String =
    runCatching { Path.of(path).toAbsolutePath().normalize().toString() }.getOrDefault(path)
