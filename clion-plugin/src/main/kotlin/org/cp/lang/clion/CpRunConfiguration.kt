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
import javax.swing.JTextField

class CpRunConfigurationType : ConfigurationTypeBase(
    ID,
    "cp",
    "从 main 函数运行 cp 源文件",
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
        ) { "运行 '${function.containingFile.name}'" }
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
    var compileOptions: String = ""
    var workingDirectory: String = project.basePath ?: ""

    override fun getConfigurationEditor(): SettingsEditor<out RunConfiguration> = CpRunConfigurationEditor(project)

    override fun checkConfiguration() {
        val source = mainFile.toPathOrNull()
            ?: throw RuntimeConfigurationError("请选择 cp main 源文件")
        if (!Files.isRegularFile(source)) {
            throw RuntimeConfigurationError("cp main 源文件不存在：$mainFile")
        }
        if (source.fileName.toString().substringAfterLast('.', "") != CpFileType.INSTANCE.defaultExtension) {
            throw RuntimeConfigurationError("cp main 源文件必须使用 .cp 扩展名")
        }
        if (CpRunPaths.resolveCompiler(project, compilerPath, source) == null) {
            throw RuntimeConfigurationError("找不到 cp 编译器；请构建编译器目标或设置编译器路径")
        }
        val resolvedWorkingDirectory = CpRunPaths.resolveWorkingDirectory(project, workingDirectory, source)
        if (!Files.isDirectory(resolvedWorkingDirectory)) {
            throw RuntimeConfigurationError("工作目录不存在：$resolvedWorkingDirectory")
        }
    }

    override fun getState(executor: Executor, environment: ExecutionEnvironment): RunProfileState =
        CpRunCommandLineState(environment, this)

    override fun readExternal(element: Element) {
        super.readExternal(element)
        mainFile = element.getAttributeValue("mainFile", "")
        compilerPath = element.getAttributeValue("compilerPath", "")
        compileOptions = element.getAttributeValue("compileOptions", "")
        workingDirectory = element.getAttributeValue("workingDirectory", project.basePath ?: "")
    }

    override fun writeExternal(element: Element) {
        super.writeExternal(element)
        element.setAttribute("mainFile", mainFile)
        element.setAttribute("compilerPath", compilerPath)
        element.setAttribute("compileOptions", compileOptions)
        element.setAttribute("workingDirectory", workingDirectory)
    }
}

private class CpRunConfigurationEditor(project: Project) : SettingsEditor<CpRunConfiguration>() {
    private val mainFileField = TextFieldWithBrowseButton()
    private val compilerPathField = TextFieldWithBrowseButton()
    private val compileOptionsField = JTextField()
    private val workingDirectoryField = TextFieldWithBrowseButton()
    private val panel = JPanel(GridBagLayout())

    init {
        mainFileField.addBrowseFolderListener(
            project,
            FileChooserDescriptorFactory.singleFile()
                .withTitle("选择 cp main 源文件")
                .withExtensionFilter("cp 源文件", CpFileType.INSTANCE.defaultExtension),
        )
        compilerPathField.addBrowseFolderListener(
            project,
            FileChooserDescriptorFactory.singleFile()
                .withTitle("选择 cp 编译器"),
        )
        workingDirectoryField.addBrowseFolderListener(
            project,
            FileChooserDescriptorFactory.createSingleFolderDescriptor()
                .withTitle("选择工作目录"),
        )
        addRow(0, "主文件：", mainFileField)
        addRow(1, "编译器路径：", compilerPathField)
        addRow(2, "编译选项：", compileOptionsField)
        addRow(3, "工作目录：", workingDirectoryField)
    }

    override fun resetEditorFrom(configuration: CpRunConfiguration) {
        mainFileField.text = configuration.mainFile
        compilerPathField.text = configuration.compilerPath
        compileOptionsField.text = configuration.compileOptions
        workingDirectoryField.text = configuration.workingDirectory
    }

    override fun applyEditorTo(configuration: CpRunConfiguration) {
        configuration.mainFile = mainFileField.text.trim()
        configuration.compilerPath = compilerPathField.text.trim()
        configuration.compileOptions = compileOptionsField.text.trim()
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
            ?: throw ExecutionException("请选择 cp main 源文件")
        val compiler = CpRunPaths.resolveCompiler(configuration.project, configuration.compilerPath, source)
            ?: throw ExecutionException("找不到 cp 编译器")
        val outputDir = Files.createTempDirectory("cp-run-")
        val executable = outputDir.resolve(source.fileName.toString().removeSuffix(".cp"))
        val stdlibRoot = CpRunPaths.resolveStdlibRoot(configuration.project, source, compiler)
        val sources = CpRunPaths.resolveRunSources(configuration.project, source, compiler)
        val workingDirectory = CpRunPaths.resolveWorkingDirectory(configuration.project, configuration.workingDirectory, source)
        val commandLine = GeneralCommandLine("bash", "-lc", buildScript(compiler, sources, executable))
            .withWorkDirectory(workingDirectory.toFile())
        CpRunPaths.resolveRuntimeLibrary(compiler)?.let {
            commandLine.withEnvironment("CP_RUNTIME_LIBRARY_PATH", it.toString())
        }
        stdlibRoot?.let {
            commandLine.withEnvironment("CP_STDLIB_ROOT_PATH", it.toString())
        }
        return OSProcessHandler(commandLine)
    }

    private fun buildScript(compiler: Path, sources: List<Path>, executable: Path): String =
        buildCpRunScript(compiler, sources, configuration.compileOptions, executable)
}

internal fun buildCpRunScript(compiler: Path, sources: List<Path>, compileOptions: String, executable: Path): String {
    val sourceArguments = sources.joinToString(" ") { shellQuote(it.toString()) }
    val compileCommand = listOf(
        shellQuote(compiler.toString()),
        compileOptions.trim(),
        sourceArguments,
        "-o",
        shellQuote(executable.toString()),
    ).filter { it.isNotBlank() }.joinToString(" ")
    return listOf(
        "set -e",
        "mkdir -p ${shellQuote(executable.parent.toString())}",
        compileCommand,
        shellQuote(executable.toString()),
    ).joinToString("\n")
}

internal object CpRunPaths {
    fun resolveCompiler(project: Project, configured: String, source: Path): Path? =
        sequenceOf(
            configured.toPathOrNull(),
            System.getProperty("cp.compiler.path").toPathOrNull(),
            System.getenv("KCP").toPathOrNull(),
            pluginNativePath("cp"),
        )
            .plus(repoCompilerCandidates(project, source))
            .plus(
                sequenceOf(
                    findExecutableOnPath("kcp"),
                    System.getProperty("user.home")?.let { Path.of(it, ".local/bin/kcp") },
                ).filterNotNull(),
            )
            .filterNotNull()
            .map { it.toAbsolutePath().normalize() }
            .firstOrNull { Files.isRegularFile(it) && Files.isExecutable(it) }

    fun resolveRunSources(project: Project, source: Path, compiler: Path): List<Path> =
        resolveSourceClosure(
            source = source,
            roots = importRoots(project, source, compiler),
            moduleSearchRoots = moduleSearchRoots(project, source, compiler),
        )

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

    internal fun resolveSourceClosure(source: Path, roots: List<Path>, moduleSearchRoots: List<Path> = roots): List<Path> {
        val normalizedSource = source.toAbsolutePath().normalize()
        val resolved = CpHelperRunner.resolveInspectionRequest(
            CpInspectionResolveRequest(
                activeFile = normalizedSource.toString(),
                targetFile = normalizedSource.toString(),
                entryFiles = listOf(normalizedSource.toString()),
                importRoots = roots.map { it.toString() },
                searchRoots = moduleSearchRoots.map { it.toString() },
                followStdlibImports = false,
            ),
        )?.files?.map { Path.of(it.path) }
        return resolved?.takeIf { it.isNotEmpty() } ?: listOf(normalizedSource)
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

    fun resolveStdlibRoot(project: Project, source: Path, compiler: Path): Path? =
        stdlibRootCandidates(project, source, compiler)
            .firstOrNull { Files.isDirectory(it) }

    fun resolveStdlibRoot(compiler: Path): Path? =
        stdlibRootCandidates(project = null, source = null, compiler)
            .firstOrNull { Files.isDirectory(it) }

    private fun importRoots(project: Project, source: Path, compiler: Path): List<Path> =
        source.parent.parentsFromSelf()
            .plus(project.basePath?.let { sequenceOf(Path.of(it)) } ?: emptySequence())
            .plus(resolveStdlibRoot(project, source, compiler)?.let { sequenceOf(it) } ?: emptySequence())
            .map { it.toAbsolutePath().normalize() }
            .distinct()
            .toList()

    private fun moduleSearchRoots(project: Project, source: Path, compiler: Path): List<Path> =
        sequenceOf(
            source.parent,
            project.basePath?.let { Path.of(it) },
            resolveStdlibRoot(project, source, compiler),
        )
            .filterNotNull()
            .map { it.toAbsolutePath().normalize() }
            .distinct()
            .toList()

    private fun stdlibRootCandidates(project: Project?, source: Path?, compiler: Path): Sequence<Path> =
        sequenceOf(System.getenv("CP_STDLIB_ROOT_PATH").toPathOrNull())
            .filterNotNull()
            .plus(project?.basePath?.let { sequenceOf(Path.of(it).resolve("std")) } ?: emptySequence())
            .plus(source?.parent.parentsFromSelf().map { it.resolve("std") })
            .plus(
                sequenceOf(
                    compiler.parent?.resolve("std"),
                    System.getProperty("user.home")?.let { Path.of(it, ".local/share/kcp/std") },
                    compiler.parent?.parent?.parent?.resolve("std"),
                ).filterNotNull(),
            )
            .map { it.toAbsolutePath().normalize() }
            .distinct()

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
