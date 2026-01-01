package tech.ula.rafaelia

object RafaeliaNative {
    init {
        System.loadLibrary("rafaelia")
    }

    external fun crc32(data: ByteArray): Int
}
