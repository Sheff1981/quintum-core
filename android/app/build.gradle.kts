plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
    id("org.jetbrains.kotlin.plugin.compose")
}

android {
    namespace = "org.quintum.wallet"
    compileSdk = 35

    defaultConfig {
        applicationId = "org.quintum.wallet"
        minSdk = 26
        targetSdk = 35
        versionCode = 1
        versionName = "0.1.0-dev"
        val sourceRevision = System.getenv("GITHUB_SHA")?.takeIf { it.matches(Regex("[0-9a-fA-F]{40}")) } ?: "local"
        buildConfigField("String", "SOURCE_REVISION", "\"$sourceRevision\"")
        testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"

        ndk {
            // Opt-in emulator testing; production builds retain their ARM64-only default.
            val testAbi = providers.gradleProperty("quintumTestAbi").orNull
            require(testAbi == null || testAbi == "x86_64") { "quintumTestAbi supports only x86_64 emulator testing" }
            abiFilters += testAbi ?: "arm64-v8a"
        }

        externalNativeBuild {
            cmake {
                arguments += listOf(
                    "-DQUINTUM_BUILD_GUI=OFF",
                    "-DQUINTUM_ENABLE_NAT_MAPPING=OFF",
                )
                targets += "quintum_android"
            }
        }
    }

    signingConfigs {
        create("quintumRelease") {
            val keyPath = System.getenv("QUM_ANDROID_RELEASE_KEYSTORE")
            if (!keyPath.isNullOrBlank()) {
                storeFile = file(keyPath)
                storePassword = System.getenv("QUM_ANDROID_RELEASE_STORE_PASSWORD")
                    ?: error("Missing release store password")
                keyAlias = System.getenv("QUM_ANDROID_RELEASE_KEY_ALIAS")
                    ?: error("Missing release key alias")
                keyPassword = System.getenv("QUM_ANDROID_RELEASE_KEY_PASSWORD")
                    ?: error("Missing release key password")
            }
        }
        create("quintumTestnet") {
            val keyPath = System.getenv("QUM_ANDROID_DEBUG_KEYSTORE")
            if (!keyPath.isNullOrBlank()) {
                storeFile = file(keyPath)
                storePassword = System.getenv("QUM_ANDROID_DEBUG_STORE_PASSWORD") ?: ""
                keyAlias = System.getenv("QUM_ANDROID_DEBUG_KEY_ALIAS") ?: "androiddebugkey"
                keyPassword = System.getenv("QUM_ANDROID_DEBUG_KEY_PASSWORD") ?: ""
            }
        }
    }

    buildTypes {
        getByName("debug") {
            signingConfig = signingConfigs.getByName("quintumTestnet")
        }
        getByName("release") {
            isMinifyEnabled = false
            if (!System.getenv("QUM_ANDROID_RELEASE_KEYSTORE").isNullOrBlank()) {
                signingConfig = signingConfigs.getByName("quintumRelease")
            }
        }
    }

    externalNativeBuild {
        cmake {
            path = file("../../CMakeLists.txt")
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

    buildFeatures {
        compose = true
        buildConfig = true
    }

    testOptions {
        unitTests.isIncludeAndroidResources = true
    }

    packaging {
        jniLibs.useLegacyPackaging = true
    }
}

dependencies {
    implementation(platform("androidx.compose:compose-bom:2025.01.00"))
    implementation("androidx.activity:activity-compose:1.10.0")
    implementation("androidx.compose.material3:material3")
    implementation("androidx.compose.ui:ui")
    implementation("androidx.lifecycle:lifecycle-runtime-ktx:2.8.7")
    implementation("androidx.core:core-ktx:1.15.0")
    implementation("org.jetbrains.kotlinx:kotlinx-coroutines-android:1.10.1")
    testImplementation("junit:junit:4.13.2")
    testImplementation("org.robolectric:robolectric:4.14.1")
    testImplementation("androidx.test:core:1.6.1")
    testImplementation("androidx.compose.ui:ui-test-junit4")
    debugImplementation("androidx.compose.ui:ui-test-manifest")
    androidTestImplementation("androidx.test:runner:1.6.2")
    androidTestImplementation("androidx.test:core:1.6.1")
    androidTestImplementation("androidx.test.ext:junit:1.2.1")
}

