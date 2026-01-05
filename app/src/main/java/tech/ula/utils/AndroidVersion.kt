package tech.ula.utils

import android.os.Build

object AndroidVersion {
    const val ANDROID_15_API = Build.VERSION_CODES.VANILLA_ICE_CREAM

    fun isAndroid15Plus(sdkInt: Int = Build.VERSION.SDK_INT): Boolean {
        return sdkInt >= ANDROID_15_API
    }
}
