package tech.ula.utils

import android.Manifest
import android.annotation.TargetApi
import android.app.Activity
import android.app.AlertDialog
import android.content.Context
import android.content.pm.PackageManager
import android.os.Build
import androidx.core.content.ContextCompat
import tech.ula.R

class PermissionHandler {
    companion object {
        private const val PERMISSION_REQUEST_CODE = 1234
        private const val NOTIFICATION_PERMISSION_REQUEST_CODE = 1235

        /**
         * Check if required storage permissions are granted.
         * For Android 13+ (API 33+), storage permissions are not needed as the app uses scoped storage.
         * For older versions, READ/WRITE_EXTERNAL_STORAGE is still required.
         */
        fun permissionsAreGranted(context: Context): Boolean {
            // Android 13+ (API 33) uses scoped storage, no need for legacy storage permissions
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                return true
            }

            return (
                ContextCompat.checkSelfPermission(
                    context,
                    Manifest.permission.READ_EXTERNAL_STORAGE,
                ) == PackageManager.PERMISSION_GRANTED &&

                    ContextCompat.checkSelfPermission(
                        context,
                        Manifest.permission.WRITE_EXTERNAL_STORAGE,
                    ) == PackageManager.PERMISSION_GRANTED
            )
        }

        /**
         * Check if POST_NOTIFICATIONS permission is granted.
         * Required for Android 13+ (API 33+) to show notifications.
         */
        fun notificationPermissionGranted(context: Context): Boolean {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                return ContextCompat.checkSelfPermission(
                    context,
                    Manifest.permission.POST_NOTIFICATIONS,
                ) == PackageManager.PERMISSION_GRANTED
            }
            // On older versions, notification permission is granted by default
            return true
        }

        fun permissionsWereGranted(
            requestCode: Int,
            grantResults: IntArray,
        ): Boolean =
            when (requestCode) {
                PERMISSION_REQUEST_CODE -> {
                    (
                        grantResults.isNotEmpty() &&
                            grantResults[0] == PackageManager.PERMISSION_GRANTED &&
                            grantResults[1] == PackageManager.PERMISSION_GRANTED
                    )
                }
                else -> false
            }

        @TargetApi(Build.VERSION_CODES.M)
        fun showPermissionsNecessaryDialog(activity: Activity) {
            // Android 13+ uses scoped storage, no need for legacy storage permissions dialog
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                return
            }

            val builder = AlertDialog.Builder(activity)
            builder
                .setMessage(R.string.alert_permissions_necessary_message)
                .setTitle(R.string.alert_permissions_necessary_title)
                .setPositiveButton(R.string.button_ok) { dialog, _ ->
                    activity.requestPermissions(
                        arrayOf(
                            Manifest.permission.READ_EXTERNAL_STORAGE,
                            Manifest.permission.WRITE_EXTERNAL_STORAGE,
                        ),
                        PERMISSION_REQUEST_CODE,
                    )
                    dialog.dismiss()
                }.setNegativeButton(R.string.alert_permissions_necessary_cancel_button) { dialog, _ ->
                    dialog.dismiss()
                }
            builder.create().show()
        }

        /**
         * Request POST_NOTIFICATIONS permission for Android 13+.
         * This should be called when the user first starts a session or when notifications are needed.
         */
        @TargetApi(Build.VERSION_CODES.TIRAMISU)
        fun requestNotificationPermission(activity: Activity) {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                activity.requestPermissions(
                    arrayOf(Manifest.permission.POST_NOTIFICATIONS),
                    NOTIFICATION_PERMISSION_REQUEST_CODE,
                )
            }
        }

        /**
         * Check if notification permission was granted in the permission result.
         */
        fun notificationPermissionWasGranted(
            requestCode: Int,
            grantResults: IntArray,
        ): Boolean =
            when (requestCode) {
                NOTIFICATION_PERMISSION_REQUEST_CODE -> {
                    grantResults.isNotEmpty() && grantResults[0] == PackageManager.PERMISSION_GRANTED
                }
                else -> false
            }
    }
}
