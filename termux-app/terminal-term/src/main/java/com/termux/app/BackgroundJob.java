package com.termux.app;

import android.util.Log;

import com.termux.terminal.term.R;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.lang.reflect.Field;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;

/**
 * A background job launched by Termux.
 */
public final class BackgroundJob {

    private static final String LOG_TAG = "termux-task";
    // Logcat truncates lines at approximately 4000 bytes, not characters.
    // With UTF-8 characters taking up to 4 bytes, use 1000 characters to
    // ensure we never exceed the byte limit even in the worst case.
    private static final int LOGCAT_MAX_LENGTH = 1000;

    final Process mProcess;

    public BackgroundJob(String cwd, String fileToExecute, final String[] args, final TermuxService service) {
        String[] env = buildEnvironment(false, cwd, service.filesPath, service.homePath, service.prefixPath);
        if (cwd == null) cwd = service.homePath;

        final String[] progArray = setupProcessArgs(fileToExecute, args, service.prefixPath);
        final String processDescription = Arrays.toString(progArray);

        Process process;
        try {
            process = Runtime.getRuntime().exec(progArray, env, new File(cwd));
        } catch (IOException e) {
            mProcess = null;
            service.showToast(service.getString(R.string.background_job_failed_to_start, processDescription));
            Log.e(LOG_TAG, "Failed running background job: " + processDescription, e);
            return;
        }

        mProcess = process;
        final int pid = getPid(mProcess);

        new Thread() {
            @Override
            public void run() {
                Log.i(LOG_TAG, "[" + pid + "] starting: " + processDescription);
                readStream(pid, "stdout", mProcess.getInputStream());

                try {
                    int exitCode = mProcess.waitFor();
                    service.onBackgroundJobExited(BackgroundJob.this);
                    if (exitCode == 0) {
                        Log.i(LOG_TAG, "[" + pid + "] exited normally");
                    } else {
                        Log.w(LOG_TAG, "[" + pid + "] exited with code: " + exitCode);
                    }
                } catch (InterruptedException e) {
                    // Ignore.
                }
            }
        }.start();


        new Thread() {
            @Override
            public void run() {
                readStream(pid, "stderr", mProcess.getErrorStream());
            }
        }.start();
    }

    private static void readStream(int pid, String streamName, InputStream stream) {
        try (BufferedReader reader = new BufferedReader(new InputStreamReader(stream, StandardCharsets.UTF_8))) {
            String line;
            while ((line = reader.readLine()) != null) {
                logLine(pid, streamName, line);
            }
        } catch (IOException e) {
            Log.e(LOG_TAG, "Error reading " + streamName + " output", e);
        }
    }

    private static void logLine(int pid, String streamName, String line) {
        String prefix = "[" + pid + "] " + streamName + ": ";
        int maxLength = LOGCAT_MAX_LENGTH - prefix.length();
        if (maxLength <= 0 || line.length() <= maxLength) {
            Log.i(LOG_TAG, prefix + line);
            return;
        }
        for (int start = 0; start < line.length(); start += maxLength) {
            int end = Math.min(line.length(), start + maxLength);
            Log.i(LOG_TAG, prefix + line.substring(start, end));
        }
    }


    public static String[] buildEnvironment(boolean failSafe, String cwd, String files_path, String home_path, String prefix_path) {
        new File(home_path).mkdirs();

        if (cwd == null) cwd = home_path;

        final String termEnv = "TERM=xterm-256color";
        final String homeEnv = "HOME=" + home_path;
        final String prefixEnv = "PREFIX=" + prefix_path;
        final String androidRootEnv = "ANDROID_ROOT=" + System.getenv("ANDROID_ROOT");
        final String androidDataEnv = "ANDROID_DATA=" + System.getenv("ANDROID_DATA");
        // EXTERNAL_STORAGE is needed for /system/bin/am to work on at least
        // Samsung S7 - see https://plus.google.com/110070148244138185604/posts/gp8Lk3aCGp3.
        final String externalStorageEnv = "EXTERNAL_STORAGE=" + System.getenv("EXTERNAL_STORAGE");
        if (failSafe) {
            // Keep the default path so that system binaries can be used in the failsafe session.
            final String pathEnv = "PATH=" + System.getenv("PATH");
            return new String[]{termEnv, homeEnv, prefixEnv, androidRootEnv, androidDataEnv, pathEnv, externalStorageEnv};
        } else {
            final String ldEnv = "LD_LIBRARY_PATH=" + files_path + "/support";
            final String langEnv = "LANG=en_US.UTF-8";
            final String pathEnv = "PATH=" + prefix_path + "/bin:" + prefix_path + "/bin/applets";
            final String pwdEnv = "PWD=" + cwd;
            final String tmpdirEnv = "TMPDIR=" + prefix_path + "/tmp";

            return new String[]{termEnv, homeEnv, prefixEnv, ldEnv, langEnv, pathEnv, pwdEnv, androidRootEnv, androidDataEnv, externalStorageEnv, tmpdirEnv};
        }
    }

    public static int getPid(Process p) {
        try {
            Field f = p.getClass().getDeclaredField("pid");
            f.setAccessible(true);
            try {
                return f.getInt(p);
            } finally {
                f.setAccessible(false);
            }
        } catch (Throwable e) {
            return -1;
        }
    }

    static String[] setupProcessArgs(String fileToExecute, String[] args, String prefix_path) {
        // The file to execute may either be:
        // - An elf file, in which we execute it directly.
        // - A script file without shebang, which we execute with our standard shell $PREFIX/bin/sh instead of the
        //   system /system/bin/sh. The system shell may vary and may not work at all due to LD_LIBRARY_PATH.
        // - A file with shebang, which we try to handle with e.g. /bin/foo -> $PREFIX/bin/foo.
        String interpreter = null;
        try {
            File file = new File(fileToExecute);
            try (FileInputStream in = new FileInputStream(file)) {
                byte[] buffer = new byte[256];
                int bytesRead = in.read(buffer);
                if (bytesRead > 4) {
                    if (buffer[0] == 0x7F && buffer[1] == 'E' && buffer[2] == 'L' && buffer[3] == 'F') {
                        // Elf file, do nothing.
                    } else if (buffer[0] == '#' && buffer[1] == '!') {
                        // Try to parse shebang.
                        StringBuilder builder = new StringBuilder();
                        for (int i = 2; i < bytesRead; i++) {
                            char c = (char) buffer[i];
                            if (c == ' ' || c == '\n') {
                                if (builder.length() == 0) {
                                    // Skip whitespace after shebang.
                                } else {
                                    // End of shebang.
                                    String executable = builder.toString();
                                    if (executable.startsWith("/usr") || executable.startsWith("/bin")) {
                                        String[] parts = executable.split("/");
                                        String binary = parts[parts.length - 1];
                                        interpreter = prefix_path + "/bin/" + binary;
                                    }
                                    break;
                                }
                            } else {
                                builder.append(c);
                            }
                        }
                    } else {
                        // No shebang and no ELF, use standard shell.
                        interpreter = prefix_path + "/bin/sh";
                    }
                }
            }
        } catch (IOException e) {
            // Ignore.
        }

        List<String> result = new ArrayList<>();
        if (interpreter != null) result.add(interpreter);
        result.add(fileToExecute);
        if (args != null) Collections.addAll(result, args);
        return result.toArray(new String[0]);
    }

}
