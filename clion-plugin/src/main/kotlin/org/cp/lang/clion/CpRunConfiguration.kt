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
    var workingDirectory: String = project.basePath ?: ""

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
        val resolvedWorkingDirectory = CpRunPaths.resolveWorkingDirectory(project, workingDirectory, source)
        if (!Files.isDirectory(resolvedWorkingDirectory)) {
            throw RuntimeConfigurationError("working directory does not exist: $resolvedWorkingDirectory")
        }
    }

    override fun getState(executor: Executor, environment: ExecutionEnvironment): RunProfileState =
        CpRunCommandLineState(environment, this)

    override fun readExternal(element: Element) {
        super.readExternal(element)
        mainFile = element.getAttributeValue("mainFile", "")
        compilerPath = element.getAttributeValue("compilerPath", "")
        workingDirectory = element.getAttributeValue("workingDirectory", project.basePath ?: "")
    }

    override fun writeExternal(element: Element) {
        super.writeExternal(element)
        element.setAttribute("mainFile", mainFile)
        element.setAttribute("compilerPath", compilerPath)
        element.setAttribute("workingDirectory", workingDirectory)
    }
}

private class CpRunConfigurationEditor(project: Project) : SettingsEditor<CpRunConfiguration>() {
    private val mainFileField = TextFieldWithBrowseButton()
    private val compilerPathField = TextFieldWithBrowseButton()
    private val workingDirectoryField = TextFieldWithBrowseButton()
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
        workingDirectoryField.addBrowseFolderListener(
            project,
            FileChooserDescriptorFactory.createSingleFolderDescriptor()
                .withTitle("Select working directory"),
        )
        addRow(0, "Main file:", mainFileField)
        addRow(1, "Compiler path:", compilerPathField)
        addRow(2, "Working directory:", workingDirectoryField)
    }

    override fun resetEditorFrom(configuration: CpRunConfiguration) {
        mainFileField.text = configuration.mainFile
        compilerPathField.text = configuration.compilerPath
        workingDirectoryField.text = configuration.workingDirectory
    }

    override fun applyEditorTo(configuration: CpRunConfiguration) {
        configuration.mainFile = mainFileField.text.trim()
        configuration.compilerPath = compilerPathField.text.trim()
        configuration.workingDirectory = workingDirectoryField.text.trim()
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
        val sources = CpRunPaths.resolveRunSources(configuration.project, source, compiler)
        val workingDirectory = CpRunPaths.resolveWorkingDirectory(configuration.project, configuration.workingDirectory, source)
        val commandLine = GeneralCommandLine("bash", "-lc", buildScript(compiler, sources, executable))
            .withWorkDirectory(workingDirectory.toFile())
        CpRunPaths.resolveRuntimeLibrary(compiler)?.let {
            commandLine.withEnvironment("CP_RUNTIME_LIBRARY_PATH", it.toString())
        }
        CpRunPaths.resolveStdlibRoot(compiler)?.let {
            commandLine.withEnvironment("CP_STDLIB_ROOT_PATH", it.toString())
        }
        return OSProcessHandler(commandLine)
    }

    private fun buildScript(compiler: Path, sources: List<Path>, executable: Path): String {
        val sourceArguments = sources.joinToString(" ") { shellQuote(it.toString()) }
        return listOf(
            "set -e",
            "mkdir -p ${shellQuote(executable.parent.toString())}",
            "${shellQuote(compiler.toString())} $sourceArguments -o ${shellQuote(executable.toString())}",
            shellQuote(executable.toString()),
        ).joinToString("\n")
    }
}

internal object CpRunPaths {
    private val importRegex = Regex("""(?m)^\s*(?:export\s+)?import\s+([A-Za-z_][A-Za-z0-9_]*(?:\.[A-Za-z_][A-Za-z0-9_]*)*)\s*;""")

    fun resolveCompiler(project: Project, configured: String, source: Path): Path? =
        sequenceOf(
            configured.toPathOrNull(),
            System.getProperty("cp.compiler.path").toPathOrNull(),
            System.getenv("KCP").toPathOrNull(),
            findExecutableOnPath("kcp"),
            System.getProperty("user.home")?.let { Path.of(it, ".local/bin/kcp") },
            pluginNativePath("cp"),
        )
            .plus(repoCompilerCandidates(project, source))
            .filterNotNull()
            .map { it.toAbsolutePath().normalize() }
            .firstOrNull { Files.isRegularFile(it) && Files.isExecutable(it) }

    fun resolveRunSources(project: Project, source: Path, compiler: Path): List<Path> =
        resolveSourceClosure(source, importRoots(project, source, compiler))

    fun resolveWorkingDirectory(project: Project, configured: String, source: Path): Path {
        configured.toPathOrNull()?.let {
            return it.toAbsolutePath().normalize()
        }
        project.basePath?.let {
            return Path.of(it).toAbsolutePath().normalize()
        }
        source.parent?.let {
            return it.toAbsolutePath().normalize()
        }
        return Path.of(".").toAbsolutePath().normalize()
    }

    internal fun resolveSourceClosure(source: Path, roots: List<Path>): List<Path> {
        val result = mutableListOf<Path>()
        val loaded = mutableSetOf<Path>()

        fun visit(path: Path) {
            val normalized = path.toAbsolutePath().normalize()
            if (!loaded.add(normalized) || !Files.isRegularFile(normalized)) {
                return
            }

            val text = runCatching { Files.readString(normalized) }.getOrNull() ?: return
            for (moduleName in importedModules(text)) {
                resolveImportPath(normalized.parent, moduleName, roots)?.let(::visit)
            }
            result.add(normalized)
        }

        visit(source)
        return result.ifEmpty { listOf(source.toAbsolutePath().normalize()) }
    }

    fun resolveRuntimeLibrary(compiler: Path): Path? =
        listOf(
            System.getenv("CP_RUNTIME_LIBRARY_PATH").toPathOrNull(),
            compiler.parent?.resolve("libcp_runtime.a"),
            System.getProperty("user.home")?.let { Path.of(it, ".local/lib/kcp/libcp_runtime.a") },
            compiler.parent?.parent?.parent?.resolve("runtime/libcp_runtime.a"),
        )
            .filterNotNull()
            .firstOrNull { Files.isRegularFile(it) }

    fun resolveStdlibRoot(compiler: Path): Path? =
        listOf(
            System.getenv("CP_STDLIB_ROOT_PATH").toPathOrNull(),
            compiler.parent?.resolve("std"),
            System.getProperty("user.home")?.let { Path.of(it, ".local/share/kcp/std") },
            compiler.parent?.parent?.parent?.resolve("std"),
        )
            .filterNotNull()
            .firstOrNull { Files.isDirectory(it) }

    private fun importRoots(project: Project, source: Path, compiler: Path): List<Path> =
        source.parent.parentsFromSelf()
            .plus(project.basePath?.let { sequenceOf(Path.of(it)) } ?: emptySequence())
            .plus(resolveStdlibRoot(compiler)?.let { sequenceOf(it) } ?: emptySequence())
            .map { it.toAbsolutePath().normalize() }
            .distinct()
            .toList()

    private fun importedModules(text: String): List<String> =
        importRegex.findAll(text).map { it.groupValues[1] }.toList()

    private fun resolveImportPath(base: Path?, moduleName: String, roots: List<Path>): Path? {
        val candidates = mutableListOf<Path>()
        if (base != null) {
            candidates.add(base.resolve(moduleLeafPath(moduleName)))
            candidates.add(base.resolve(moduleRelativePath(moduleName)))
            aggregateRelativePath(moduleName)?.let { candidates.add(base.resolve(it)) }
        }
        for (root in roots) {
            candidates.add(root.resolve(moduleRelativePath(moduleName)))
            aggregateRelativePath(moduleName)?.let { candidates.add(root.resolve(it)) }
            stdlibRelativePath(moduleName)?.let { candidates.add(root.resolve(it)) }
        }
        return candidates
            .asSequence()
            .map { it.toAbsolutePath().normalize() }
            .firstOrNull { Files.isRegularFile(it) }
    }

    private fun moduleRelativePath(moduleName: String): Path {
        val components = moduleName.split(".")
        var path = Path.of("")
        for ((index, component) in components.withIndex()) {
            path = path.resolve(if (index == components.lastIndex) "$component.cp" else component)
        }
        return path
    }

    private fun aggregateRelativePath(moduleName: String): Path? {
        val components = moduleName.split(".")
        var path = Path.of("")
        for (component in components) {
            path = path.resolve(component)
        }
        return path.resolve("${components.last()}.cp")
    }

    private fun moduleLeafPath(moduleName: String): Path =
        Path.of("${moduleName.substringAfterLast('.')}.cp")

    private fun stdlibRelativePath(moduleName: String): Path? =
        moduleName.removePrefix("std.").takeIf { it != moduleName }?.let { moduleRelativePath(it) }

    private fun findExecutableOnPath(name: String): Path? =
        System.getenv("PATH")
            ?.splitToSequence(java.io.File.pathSeparator)
            ?.map { Path.of(it).resolve(name) }
            ?.firstOrNull { Files.isRegularFile(it) && Files.isExecutable(it) }

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
