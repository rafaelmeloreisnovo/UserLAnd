package tech.ula.utils

import android.os.Build

object AndroidVersion {
    const val ANDROID_15_API = Build.VERSION_CODES.VANILLA_ICE_CREAM

    fun isAndroid10Plus(sdkInt: Int = Build.VERSION.SDK_INT) = sdkInt >= Build.VERSION_CODES.Q
    fun isAndroid11Plus(sdkInt: Int = Build.VERSION.SDK_INT) = sdkInt >= Build.VERSION_CODES.R
    fun isAndroid12Plus(sdkInt: Int = Build.VERSION.SDK_INT) = sdkInt >= Build.VERSION_CODES.S
    fun isAndroid13Plus(sdkInt: Int = Build.VERSION.SDK_INT) = sdkInt >= Build.VERSION_CODES.TIRAMISU
    fun isAndroid14Plus(sdkInt: Int = Build.VERSION.SDK_INT) = sdkInt >= Build.VERSION_CODES.UPSIDE_DOWN_CAKE
    fun isAndroid15Plus(sdkInt: Int = Build.VERSION.SDK_INT) = sdkInt >= ANDROID_15_API
}
