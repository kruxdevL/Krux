import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.regex.Matcher;
import java.util.regex.Pattern;
import java.util.zip.Adler32;

public final class ExtractRicoPixel {
    private byte[] input;
    private byte[] output;
    private int in;
    private int out;

    private int in2(int at) {
        return (input[at] & 0xff) << 8 | input[at + 1] & 0xff;
    }

    private int in3(int at) {
        return (input[at] & 0xff) << 16 | in2(at + 1);
    }

    private long in4(int at) {
        return (long) (input[at] & 0xff) << 24 | (long) in3(at + 1);
    }

    private void literal(int at, int length) {
        System.arraycopy(input, at, output, out, length);
        out += length;
    }

    private void match(int distance, int length) {
        int source = out - distance;
        for (int index = 0; index < length; ++index) output[out++] = output[source++];
    }

    private void token() {
        int value = input[in] & 0xff;
        if (value >= 0x80) {
            match((input[in + 1] & 0xff) + 1, value - 0x80 + 1);
            in += 2;
        } else if (value >= 0x40) {
            match(in2(in) - 0x4000 + 1, (input[in + 2] & 0xff) + 1);
            in += 3;
        } else if (value >= 0x20) {
            int length = value - 0x20 + 1;
            literal(in + 1, length);
            in += 1 + length;
        } else if (value >= 0x18) {
            match(in3(in) - 0x180000 + 1, (input[in + 3] & 0xff) + 1);
            in += 4;
        } else if (value >= 0x10) {
            match(in3(in) - 0x100000 + 1, in2(in + 3) + 1);
            in += 5;
        } else if (value >= 0x08) {
            int length = in2(in) - 0x0800 + 1;
            literal(in + 2, length);
            in += 2 + length;
        } else if (value == 0x07) {
            int length = in2(in + 1) + 1;
            literal(in + 3, length);
            in += 3 + length;
        } else if (value == 0x06) {
            match(in3(in + 1) + 1, (input[in + 4] & 0xff) + 1);
            in += 5;
        } else if (value == 0x04) {
            match(in3(in + 1) + 1, in2(in + 4) + 1);
            in += 6;
        } else {
            throw new IllegalStateException("bad token at " + in);
        }
    }

    private byte[] decompress(byte[] compressed) {
        input = compressed;
        if (in4(0) != 0x57bc0000L || in4(4) != 0) throw new IllegalArgumentException("not stb data");
        output = new byte[(int) in4(8)];
        in = 16;
        out = 0;
        while (!(input[in] == 0x05 && (input[in + 1] & 0xff) == 0xfa)) token();
        if (out != output.length) throw new IllegalStateException("size mismatch");
        Adler32 adler = new Adler32();
        adler.update(output);
        if (adler.getValue() != in4(in + 2)) throw new IllegalStateException("checksum mismatch");
        return output;
    }

    private static byte[] parse(Path source) throws IOException {
        String text = Files.readString(source);
        int begin = text.indexOf("Pixel_compressed_data");
        begin = text.indexOf('{', begin);
        int end = text.indexOf("};", begin);
        Matcher matcher = Pattern.compile("0x[0-9a-fA-F]+").matcher(text.substring(begin, end));
        ArrayList<Integer> words = new ArrayList<>();
        while (matcher.find()) words.add((int) Long.parseLong(matcher.group().substring(2), 16));
        ByteBuffer bytes = ByteBuffer.allocate(words.size() * 4).order(ByteOrder.LITTLE_ENDIAN);
        for (int word : words) bytes.putInt(word);
        byte[] padded = bytes.array();
        byte[] compressed = new byte[10434];
        System.arraycopy(padded, 0, compressed, 0, compressed.length);
        return compressed;
    }

    public static void main(String[] arguments) throws Exception {
        if (arguments.length < 2 || arguments.length > 3)
            throw new IllegalArgumentException("fonts.hpp output.ttf [coldwtf.assets]");
        byte[] font = new ExtractRicoPixel().decompress(parse(Path.of(arguments[0])));
        Files.write(Path.of(arguments[1]), font);
        System.out.printf("wrote %d bytes%n", font.length);
        if (arguments.length == 3) {
            Path asset = Path.of(arguments[2]);
            byte[] current = Files.readAllBytes(asset);
            if (current.length == 400392) {
                byte[] packed = new byte[current.length + font.length];
                System.arraycopy(current, 0, packed, 0, current.length);
                System.arraycopy(font, 0, packed, current.length, font.length);
                Files.write(asset, packed);
            } else if (current.length != 425992) {
                throw new IllegalStateException("unexpected asset size " + current.length);
            }
            System.out.printf("asset blob %d bytes%n", Files.size(asset));
        }
    }
}