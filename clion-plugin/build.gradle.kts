import org.gradle.api.tasks.Exec
import org.gradle.api.tasks.bundling.Zip
import org.gradle.api.tasks.testing.Test
import org.jetbrains.intellij.platform.gradle.TestFrameworkType
import org.jetbrains.kotlin.gradle.dsl.JvmTarget
import java.nio.file.Files
import java.nio.file.StandardCopyOption

plugins {
    id("java")
    kotlin("jvm") version "2.3.0"
    kotlin("plugin.serialization") version "2.3.0"
    id("org.jetbrains.intellij.platform") version "2.13.1"
}

group = providers.gradleProperty("pluginGroup").get()
version = providers.gradleProperty("pluginVersion").get()

val pluginId = providers.gradleProperty("pluginId").get()
val pluginName = providers.gradleProperty("pluginName").get()
val repoRoot = layout.projectDirectory.dir("..")
val nativeBuildDir = repoRoot.dir("build-clion-plugin-native")
val nativeHelperPath = nativeBuildDir.file("artifacts/native/linux-x86_64/cp-lexer-helper")
val stagedNativeDir = layout.buildDirectory.dir("staged-native/linux-x86_64")
val stagedNativeHelper = stagedNativeDir.map { it.file("cp-lexer-helper") }
val clionLocalPath = providers.gradleProperty("clionLocalPath")
    .orElse(providers.environmentVariable("CLION_HOME"))
    .get()

kotlin {
    jvmToolchain(21)
    compilerOptions {
        jvmTarget.set(JvmTarget.JVM_21)
    }
}

repositories {
    mavenCentral()

    intellijPlatform {
        defaultRepositories()
        localPlatformArtifacts()
    }
}

dependencies {
    testImplementation("junit:junit:4.13.2")

    intellijPlatform {
        local(clionLocalPath)
        testFramework(TestFrameworkType.Platform)
    }
}

intellijPlatform {
    pluginConfiguration {
        name = providers.gradleProperty("pluginName")
        version = providers.gradleProperty("pluginVersion")
        description = """
            Rich highlighting and parser/semantic diagnostics for the cp experimental language.
            The editor lexer runs inside CLion and project-level frontend analysis is delegated to the native cp helper.
        """.trimIndent()

        ideaVersion {
            sinceBuild = providers.gradleProperty("pluginSinceBuild")
            untilBuild = providers.gradleProperty("pluginUntilBuild")
        }
    }
}

val configureNativeHelper by tasks.registering(Exec::class) {
    group = "native"
    description = "Configure the native cp lexer helper build."

    inputs.file(repoRoot.file("CMakeLists.txt"))
    inputs.dir(repoRoot.dir("diagnostic"))
    inputs.dir(repoRoot.dir("lexer"))
    inputs.dir(repoRoot.dir("parser"))
    inputs.dir(repoRoot.dir("preprocessor"))
    inputs.dir(repoRoot.dir("semantic"))
    inputs.dir(repoRoot.dir("clion-plugin/native"))
    outputs.file(nativeBuildDir.file("CMakeCache.txt"))

    commandLine(
        "cmake",
        "-S", repoRoot.asFile.absolutePath,
        "-B", nativeBuildDir.asFile.absolutePath,
        "-G", "Ninja",
        "-DCMAKE_BUILD_TYPE=Debug",
    )
}

val buildNativeHelper by tasks.registering(Exec::class) {
    group = "native"
    description = "Build the native cp lexer helper."

    dependsOn(configureNativeHelper)
    inputs.file(nativeBuildDir.file("CMakeCache.txt"))
    inputs.dir(repoRoot.dir("clion-plugin/native"))
    inputs.dir(repoRoot.dir("parser"))
    inputs.dir(repoRoot.dir("semantic"))
    outputs.file(nativeHelperPath)

    commandLine(
        "cmake",
        "--build", nativeBuildDir.asFile.absolutePath,
        "--target", "cp_lexer_helper_cli",
        "-j4",
    )
}

val stageNativeHelper by tasks.registering {
    group = "native"
    description = "Stage the native cp lexer helper for plugin packaging."

    dependsOn(buildNativeHelper)
    inputs.file(nativeHelperPath)
    outputs.file(stagedNativeHelper)

    doLast {
        val source = nativeHelperPath.asFile.toPath()
        val target = stagedNativeHelper.get().asFile.toPath()
        Files.createDirectories(target.parent)
        Files.copy(source, target, StandardCopyOption.REPLACE_EXISTING, StandardCopyOption.COPY_ATTRIBUTES)
        target.toFile().setExecutable(true)
    }
}

tasks.named("prepareSandbox").configure {
    dependsOn(stageNativeHelper)

    doLast {
        val source = stagedNativeHelper.get().asFile.toPath()
        val targetDir = layout.buildDirectory.dir("idea-sandbox/plugins/$pluginName/native/linux-x86_64")
            .get()
            .asFile
            .toPath()
        Files.createDirectories(targetDir)
        val target = targetDir.resolve("cp-lexer-helper")
        Files.copy(source, target, StandardCopyOption.REPLACE_EXISTING, StandardCopyOption.COPY_ATTRIBUTES)
        target.toFile().setExecutable(true)
    }
}

tasks.named<Test>("test") {
    dependsOn(buildNativeHelper)
    useJUnit()
    systemProperty("cp.helper.path", nativeHelperPath.asFile.absolutePath)
    systemProperty("cp.repo.root", repoRoot.asFile.absolutePath)
}

tasks.named<Zip>("buildPlugin") {
    dependsOn(stageNativeHelper)

    from(stagedNativeHelper) {
        into("native/linux-x86_64")
        filePermissions {
            unix("rwxr-xr-x")
        }
    }
}

tasks.named("buildSearchableOptions").configure {
    enabled = false
}
