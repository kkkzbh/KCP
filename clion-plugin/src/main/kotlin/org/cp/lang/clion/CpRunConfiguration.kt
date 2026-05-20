package org.cp.lang.clion

import com.intellij.execution.ExecutionException
import com.intellij.execution.Executor
import com.intellij.execution.actions.ConfigurationContext
import com.intellij.execution.actions.LazyRunConfigurationProducer
import com.intellij.execution.configurations.CommandLineState
import com.intellij.execution.configurations.ConfigurationFactory
import com.intellij.execution.configurations.ConfigurationTypeBase
import com.intellij.execution.configurations.ConfigurationTypeUtil
import com.intellij.execution.configurations.GeneralCommandLine
import com.intellij.execution.configurations.RunConfiguration
import com.intellij.execution.configurations.RunConfigurationBase
import com.intellij.execution.configurations.RunConfigurationOptions
import com.intellij.execution.configurations.RunProfileState
import com.intellij.execution.configurations.RuntimeConfigurationError
import com.intellij.execution.lineMarker.ExecutorAction
import com.intellij.execution.lineMarker.RunLineMarkerContributor
import com.intellij.execution.process.OSProcessHandler
import com.intellij.execution.process.ProcessHandler
import com.intellij.execution.runners.ExecutionEnvironment
import com.intellij.icons.AllIcons
import com.intellij.openapi.fileChooser.FileChooserDescriptorFactory
import com.intellij.openapi.options.SettingsEditor
import com.intellij.openapi.project.Project
import com.intellij.openapi.ui.TextFieldWithBrowseButton
import com.intellij.openapi.util.Ref
import com.intellij.openapi.util.NotNullLazyValue
import com.intellij.openapi.vfs.LocalFileSystem
import com.intellij.psi.PsiElement
import com.intellij.ui.components.JBLabel
import org.jdom.Element
import java.awt.GridBagConstraints
import java.awt.GridBagLayout
import java.nio.file.Files
import java.nio.file.Path
import javax.swing.JComponent
import javax.swing.JPanel

class CpRunConfigurationType : ConfigurationTypeBase(
    ID,
    "cp",
    "Run a cp source file from its main function",
    NotNullLazyValue.createValue { AllIcons.RunConfigurations.Application },
) {
    init {
        addFactory(CpRunConfigurationFactory(this))
    }

    companion object {
        const val ID: String = "CpRunConfiguration"
    }
}

class CpRunConfigurationProducer : LazyRunConfigurationProducer<CpRunConfiguration>() {
    override fun setupConfigurationFromContext(
        configuration: CpRunConfiguration,
        context: ConfigurationContext,
        sourceElement: Ref<PsiElement>,
    ): Boolean {
        val main = context.psiLocation?.cpTopLevelMainFunction() ?: return false
        val path = main.containingFile?.virtualFile?.path ?: return false

        configuration.mainFile = path
        configuration.name = main.containingFile.name.removeSuffix(".cp")
        sourceElement.set(main.firstDescendant(CpElements.FUNCTION_NAME) ?: main)
        return true
    }

    override fun isConfigurationFromContext(configuration: CpRunConfiguration, context: ConfigurationContext): Boolean {
        val main = context.psiLocation?.cpTopLevelMainFunction() ?: return false
        val filePath = main.containingFile?.virtualFile?.path ?: return false
        val path = Path.of(filePath).toAbsolutePath().normalize()
        val configured = configuration.mainFile.toPathOrNull()?.toAbsolutePath()?.normalize() ?: return false
        return path == configured
    }

    override fun getConfigurationFactory(): ConfigurationFactory =
        cpRunConfigurationFactory()
}

class CpRunLineMarkerContributor : RunLineMarkerContributor() {
    override fun getInfo(element: PsiElement): Info? {
        if (element.cpElementType() != CpTypes.IDENTIFIER || element.text != "main") {
            return null
        }
        val functionName = element.parent.takeIf { it?.cpElementType() == CpElements.FUNCTION_NAME } ?: return null
        val function = functionName.parent.takeIf { it?.cpTopLevelMainFunction() != null } ?: return null
        if(function.firstDescendant(CpElements.FUNCTION_NAME) != functionName) {
            return null
        }

        return Info(
            AllIcons.RunConfigurations.TestState.Run,
            ExecutorAction.getActions(0),
        ) { "Run '${function.containingFile.name}'" }
    }
}

private class CpRunConfigurationFactory(type: CpRunConfigurationType) : ConfigurationFactory(type) {
    override fun getId(): String = CpRunConfigurationType.ID

    override fun createTemplateConfiguration(project: Project): RunConfiguration =
        CpRunConfiguration(project, this, "main")
}

private fun cpRunConfigurationFactory(): ConfigurationFactory =
    ConfigurationTypeUtil.findConfigurationType(CpRunConfigurationType::class.java).configurationFactories.single()

class CpRunConfiguration(project: Project, factory: ConfigurationFactory, name: String) :
    RunConfigurationBase<RunConfigurationOptions>(project, factory, name) {
    var mainFile: String = ""
    var compilerPath: String = ""

    override fun getConfigurationEditor(): SettingsEditor<out RunConfiguration> = CpRunConfigurationEditor(project)

    override fun checkConfiguration() {
        val source = mainFile.toPathOrNull()
            ?: throw RuntimeConfigurationError("Choose a cp main source file")
        if (!Files.isRegularFile(source)) {
            throw RuntimeConfigurationError("cp main source file does not exist: $mainFile")
        }
        if (source.fileName.toString().substringAfterLast('.', "") != CpFileType.INSTANCE.defaultExtension) {
            throw RuntimeConfigurationError("cp main source file must use .cp extension")
        }
        if (CpRunPaths.resolveCompiler(project, compilerPath, source) == null) {
            throw RuntimeConfigurationError("cp compiler was not found; build the compiler target or set a compiler path")
        }
    }

    override fun getState(executor: Executor, environment: ExecutionEnvironment): RunProfileState =
        CpRunCommandLineState(environment, this)

    override fun readExternal(element: Element) {
        super.readExternal(element)
        mainFile = element.getAttributeValue("mainFile", "")
        compilerPath = element.getAttributeValue("compilerPath", "")
    }

    override fun writeExternal(element: Element) {
        super.writeExternal(element)
        element.setAttribute("mainFile", mainFile)
        element.setAttribute("compilerPath", compilerPath)
    }
}

private class CpRunConfigurationEditor(project: Project) : SettingsEditor<CpRunConfiguration>() {
    private val mainFileField = TextFieldWithBrowseButton()
    private val compilerPathField = TextFieldWithBrowseButton()
    private val panel = JPanel(GridBagLayout())

    init {
        mainFileField.addBrowseFolderListener(
            project,
            FileChooserDescriptorFactory.singleFile()
                .withTitle("Select cp main source file")
                .withExtensionFilter("cp source file", CpFileType.INSTANCE.defaultExtension),
        )
        compilerPathField.addBrowseFolderListener(
            project,
            FileChooserDescriptorFactory.singleFile()
                .withTitle("Select cp compiler"),
        )
        addRow(0, "Main file:", mainFileField)
        addRow(1, "Compiler path:", compilerPathField)
    }

    override fun resetEditorFrom(configuration: CpRunConfiguration) {
        mainFileField.text = configuration.mainFile
        compilerPathField.text = configuration.compilerPath
    }

    override fun applyEditorTo(configuration: CpRunConfiguration) {
        configuration.mainFile = mainFileField.text.trim()
        configuration.compilerPath = compilerPathField.text.trim()
    }

    override fun createEditor(): JComponent = panel

    private fun addRow(row: Int, label: String, component: JComponent) {
        panel.add(
            JBLabel(label),
            GridBagConstraints().apply {
                gridx = 0
                gridy = row
                anchor = GridBagConstraints.WEST
                insets.set(4, 0, 4, 8)
            },
        )
        panel.add(
            component,
            GridBagConstraints().apply {
                gridx = 1
                gridy = row
                weightx = 1.0
                fill = GridBagConstraints.HORIZONTAL
                insets.set(4, 0, 4, 0)
            },
        )
    }
}

private class CpRunCommandLineState(environment: ExecutionEnvironment, private val configuration: CpRunConfiguration) :
    CommandLineState(environment) {
    override fun startProcess(): ProcessHandler {
        val source = configuration.mainFile.toPathOrNull()
            ?: throw ExecutionException("Choose a cp main source file")
        val compiler = CpRunPaths.resolveCompiler(configuration.project, configuration.compilerPath, source)
            ?: throw ExecutionException("cp compiler was not found")
        val outputDir = Files.createTempDirectory("cp-run-")
        val executable = outputDir.resolve(source.fileName.toString().removeSuffix(".cp"))
        val commandLine = GeneralCommandLine("bash", "-lc", buildScript(compiler, source, executable))
            .withWorkDirectory(source.parent?.toFile())
        CpRunPaths.resolveRuntimeLibrary(compiler)?.let {
            commandLine.withEnvironment("CP_RUNTIME_LIBRARY_PATH", it.toString())
        }
        CpRunPaths.resolveStdlibRoot(compiler)?.let {
            commandLine.withEnvironment("CP_STDLIB_ROOT_PATH", it.toString())
        }
        return OSProcessHandler(commandLine)
    }

    private fun buildScript(compiler: Path, source: Path, executable: Path): String =
        listOf(
            "set -e",
            "mkdir -p ${shellQuote(executable.parent.toString())}",
            "${shellQuote(compiler.toString())} -o ${shellQuote(executable.toString())} ${shellQuote(source.toString())}",
            shellQuote(executable.toString()),
        ).joinToString("\n")
}

internal object CpRunPaths {
    fun resolveCompiler(project: Project, configured: String, source: Path): Path? =
        sequenceOf(
            configured.toPathOrNull(),
            System.getProperty("cp.compiler.path").toPathOrNull(),
            pluginNativePath("cp"),
        )
            .plus(repoCompilerCandidates(project, source))
            .filterNotNull()
            .map { it.toAbsolutePath().normalize() }
            .firstOrNull { Files.isRegularFile(it) }

    fun resolveRuntimeLibrary(compiler: Path): Path? =
        listOf(
            compiler.parent?.resolve("libcp_runtime.a"),
            compiler.parent?.parent?.parent?.resolve("runtime/libcp_runtime.a"),
        )
            .filterNotNull()
            .firstOrNull { Files.isRegularFile(it) }

    fun resolveStdlibRoot(compiler: Path): Path? =
        listOf(
            compiler.parent?.resolve("std"),
            compiler.parent?.parent?.parent?.resolve("std"),
        )
            .filterNotNull()
            .firstOrNull { Files.isDirectory(it) }

    private fun repoCompilerCandidates(project: Project, source: Path): Sequence<Path> =
        listOfNotNull(project.basePath?.let { Path.of(it) }, source.parent)
            .asSequence()
            .flatMap { directory ->
                directory.parentsFromSelf()
                    .flatMap { root ->
                        sequenceOf(
                            root.resolve("build-clion-plugin-native/compiler/cp"),
                            root.resolve("build/compiler/cp"),
                            root.resolve("cmake-build-debug/compiler/cp"),
                        )
                    }
            }

    private fun pluginNativePath(name: String): Path? {
        val classLocation = runCatching {
            Path.of(CpRunPaths::class.java.protectionDomain.codeSource.location.toURI())
        }.getOrNull() ?: return null
        val pluginRoot = when {
            Files.isRegularFile(classLocation) -> classLocation.parent?.parent
            else -> classLocation
        } ?: return null
        return pluginRoot.resolve("native/linux-x86_64").resolve(name)
    }
}

private fun Path.parentsFromSelf(): Sequence<Path> =
    generateSequence(toAbsolutePath().normalize()) { it.parent }

private fun PsiElement.cpTopLevelMainFunction(): PsiElement? {
    val function = if (cpElementType() == CpElements.FUNCTION) {
        this
    } else {
        parentByType(CpElements.FUNCTION)
    } ?: return null
    val name = function.firstDescendant(CpElements.FUNCTION_NAME) ?: return null
    if (name.text != "main") {
        return null
    }
    if (function.parentByType(CpElements.IMPL_BLOCK) != null || function.parentByType(CpElements.CONCEPT_DECLARATION) != null) {
        return null
    }
    return function
}

private fun String?.toPathOrNull(): Path? =
    takeUnless { it.isNullOrBlank() }?.let {
        LocalFileSystem.getInstance().refreshAndFindFileByPath(it)
        Path.of(it)
    }

private fun shellQuote(value: String): String =
    "'" + value.replace("'", "'\\''") + "'"
