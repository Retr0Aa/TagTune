import java.awt.RenderingHints
import java.awt.image.BufferedImage
import java.io.ByteArrayOutputStream
import java.io.File
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.util.jar.JarFile
import javax.imageio.ImageIO

plugins {
    id("java")
    application
}

group = "org.retr0a"
version = "1.0-SNAPSHOT"

repositories {
    mavenCentral()
}

dependencies {
    implementation("net.jthink:jaudiotagger:3.0.1")
    testImplementation(platform("org.junit:junit-bom:5.10.0"))
    testImplementation("org.junit.jupiter:junit-jupiter")
    testRuntimeOnly("org.junit.platform:junit-platform-launcher")
}

application {
    mainClass.set("org.retr0a.tagtune.Main")
    applicationDefaultJvmArgs = listOf("-Xdock:name=TagTune")
}

fun packagedJarFile(libsDir: File): File {
    val fatJar = libsDir.listFiles()
        ?.firstOrNull { it.name.endsWith("-all.jar") }
        ?: throw RuntimeException("Fat jar not found in ${libsDir.absolutePath}")

    // Verify a key dependency is actually bundled before jpackage runs.
    JarFile(fatJar).use { jar ->
        if (jar.getEntry("org/jaudiotagger/audio/AudioFileIO.class") == null) {
            throw RuntimeException("Packaged jar ${fatJar.name} is missing jaudiotagger classes")
        }
    }

    return fatJar
}

tasks.register<Jar>("fatJar") {
    archiveClassifier.set("all")
    duplicatesStrategy = DuplicatesStrategy.EXCLUDE

    from(sourceSets.main.get().output)
    dependsOn(configurations.runtimeClasspath)
    from({
        configurations.runtimeClasspath.get()
            .filter { it.name.endsWith(".jar") }
            .map { zipTree(it) }
    })
}

fun commandExists(command: String): Boolean {
    val path = System.getenv("PATH") ?: return false
    val pathEntries = path.split(File.pathSeparatorChar)
    val executableNames = if (System.getProperty("os.name").lowercase().contains("win")) {
        listOf(command, "$command.exe", "$command.cmd", "$command.bat")
    } else {
        listOf(command)
    }

    return pathEntries.any { entry ->
        executableNames.any { name -> File(entry, name).isFile }
    }
}

data class CommandResult(
    val exitCode: Int,
    val output: String
)

fun runCommand(vararg command: String): CommandResult {
    val output = ByteArrayOutputStream()
    val process = ProcessBuilder(*command)
        .redirectErrorStream(true)
        .start()

    process.inputStream.use { input -> input.copyTo(output) }

    return CommandResult(
        exitCode = process.waitFor(),
        output = output.toString().trim()
    )
}

fun hasWixToolset(): Boolean = commandExists("wix") || (commandExists("candle") && commandExists("light"))

fun ensureWixToolsetUsable() {
    if (!commandExists("wix")) {
        return
    }

    val wixVersion = runCommand("wix", "--version").output.lineSequence().firstOrNull().orEmpty()
    val wixHelp = runCommand("wix", "build", "-h")

    if (wixHelp.output.contains("error WIX7015")) {
        throw GradleException(
            "packageExe found WiX ${if (wixVersion.isNotBlank()) wixVersion else "v7"} on PATH, " +
            "but WiX 7 refuses to run until its OSMF EULA is accepted. " +
            "Run `wix eula accept wix7` once, or install WiX 6/5/4 on PATH instead. " +
            "If you want a no-installer bundle instead, run packageAppImageWindows."
        )
    }
}

tasks.register<Exec>("createIcns") {
    val iconPng = file("src/main/resources/Icon-iOS-Default-1024x1024@1x.png")
    val iconsetDir = layout.buildDirectory.dir("TagTune.iconset")
    val icnsFile = layout.buildDirectory.file("TagTune.icns")

    inputs.file(iconPng)
    outputs.file(icnsFile)

    commandLine("sh", "-c", """
        mkdir -p "${iconsetDir.get().asFile.absolutePath}"
        for size in 16 32 128 256 512; do
            sips -z ${'$'}size ${'$'}size "${iconPng.absolutePath}" --out "${iconsetDir.get().asFile.absolutePath}/icon_${'$'}{size}x${'$'}{size}.png"
            size2=$((size * 2))
            sips -z ${'$'}size2 ${'$'}size2 "${iconPng.absolutePath}" --out "${iconsetDir.get().asFile.absolutePath}/icon_${'$'}{size}x${'$'}{size}@2x.png"
        done
        iconutil -c icns "${iconsetDir.get().asFile.absolutePath}" -o "${icnsFile.get().asFile.absolutePath}"
    """.trimIndent())
}

tasks.register("createIco") {
    val iconPng = file("src/main/resources/Icon-Default.png")
    val icoFile = layout.buildDirectory.file("TagTune.ico")

    inputs.file(iconPng)
    outputs.file(icoFile)

    doLast {
        val source = ImageIO.read(iconPng) ?: throw RuntimeException("Unable to read ${iconPng.absolutePath}")
        val resized = BufferedImage(256, 256, BufferedImage.TYPE_INT_ARGB)
        val graphics = resized.createGraphics()
        try {
            graphics.setRenderingHint(RenderingHints.KEY_INTERPOLATION, RenderingHints.VALUE_INTERPOLATION_BICUBIC)
            graphics.setRenderingHint(RenderingHints.KEY_RENDERING, RenderingHints.VALUE_RENDER_QUALITY)
            graphics.setRenderingHint(RenderingHints.KEY_ANTIALIASING, RenderingHints.VALUE_ANTIALIAS_ON)
            graphics.drawImage(source, 0, 0, 256, 256, null)
        } finally {
            graphics.dispose()
        }

        val pngBytes = ByteArrayOutputStream().use { output ->
            ImageIO.write(resized, "png", output)
            output.toByteArray()
        }

        val header = ByteBuffer.allocate(22).order(ByteOrder.LITTLE_ENDIAN).apply {
            putShort(0)
            putShort(1)
            putShort(1)
            put(0)
            put(0)
            put(0)
            put(0)
            putShort(1)
            putShort(32)
            putInt(pngBytes.size)
            putInt(22)
        }.array()

        icoFile.get().asFile.outputStream().use { output ->
            output.write(header)
            output.write(pngBytes)
        }
    }
}

tasks.register<Exec>("packageDmg") {
    dependsOn("fatJar", "createIcns")

    val libsDir = layout.buildDirectory.dir("libs")
    val icnsFile = layout.buildDirectory.file("TagTune.icns")
    val buildDir = layout.buildDirectory

    doFirst {
        val jarFile = packagedJarFile(libsDir.get().asFile)

        commandLine(
            "jpackage",
            "--input", libsDir.get().asFile.absolutePath,
            "--name", "TagTune",
            "--main-jar", jarFile.name,
            "--main-class", "org.retr0a.tagtune.Main",
            "--type", "dmg",
            "--icon", icnsFile.get().asFile.absolutePath,
            "--java-options", "-Xdock:name=TagTune",
            "--dest", buildDir.get().asFile.absolutePath
        )
    }
}

tasks.register<Exec>("packageAppImageWindows") {
    dependsOn("fatJar", "createIco")

    val libsDir = layout.buildDirectory.dir("libs")
    val icoFile = layout.buildDirectory.file("TagTune.ico")
    val buildDir = layout.buildDirectory

    doFirst {
        if (!System.getProperty("os.name").lowercase().contains("win")) {
            throw GradleException("packageAppImageWindows must be run on Windows because jpackage does not support cross-platform packaging.")
        }

        val jarFile = packagedJarFile(libsDir.get().asFile)

        commandLine(
            "jpackage",
            "--input", libsDir.get().asFile.absolutePath,
            "--name", "TagTune",
            "--main-jar", jarFile.name,
            "--main-class", "org.retr0a.tagtune.Main",
            "--type", "app-image",
            "--icon", icoFile.get().asFile.absolutePath,
            "--dest", buildDir.get().asFile.absolutePath
        )
    }
}

tasks.register<Exec>("packageExe") {
    dependsOn("fatJar", "createIco")

    val libsDir = layout.buildDirectory.dir("libs")
    val icoFile = layout.buildDirectory.file("TagTune.ico")
    val buildDir = layout.buildDirectory

    doFirst {
        if (!System.getProperty("os.name").lowercase().contains("win")) {
            throw GradleException("packageExe must be run on Windows because jpackage does not support cross-platform packaging.")
        }
        if (!hasWixToolset()) {
            throw GradleException(
                "packageExe requires WiX on Windows. Install WiX v4/v5 (wix.exe) or WiX v3 (candle.exe and light.exe) and ensure it is on PATH. " +
                "If you want a no-installer bundle instead, run packageAppImageWindows."
            )
        }
        ensureWixToolsetUsable()

        val jarFile = packagedJarFile(libsDir.get().asFile)

        commandLine(
            "jpackage",
            "--input", libsDir.get().asFile.absolutePath,
            "--name", "TagTune",
            "--main-jar", jarFile.name,
            "--main-class", "org.retr0a.tagtune.Main",
            "--type", "exe",
            "--icon", icoFile.get().asFile.absolutePath,
            "--win-menu",
            "--win-shortcut",
            "--win-dir-chooser",
            "--dest", buildDir.get().asFile.absolutePath
        )
    }
}

tasks.test {
    useJUnitPlatform()
}
