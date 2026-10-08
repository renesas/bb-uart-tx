import re
import time
import serial
from robot.api.deco import library, keyword


@library
class UartLibrary:
    @keyword("Capture Unity Output After Reset")
    def capture_unity_output_after_reset(self, port, baud, timeout_sec, build_dir):

        import subprocess
        import time

        timeout_sec = float(timeout_sec)
        deadline = time.time() + timeout_sec

        buf = ""

        # Open serial FIRST
        ser = serial.Serial(port=port, baudrate=int(baud), timeout=0.2)

        try:
            # IMPORTANT: small delay to stabilize port
            time.sleep(0.5)

            # Now trigger reset
            subprocess.run(
                ["ninja", "-C", build_dir, "rfp_reset"],
                check=True
            )

            # Give MCU time to boot
            time.sleep(0.2)

            summary_re = re.compile(r'(\d+)\s+Tests\s+(\d+)\s+Failures\s+(\d+)')
            fail_re = re.compile(r'\bFAIL\b')
            ok_re = re.compile(r'\bOK\b|\bPASS\b')

            while time.time() < deadline:
                chunk = ser.read(512)
                if chunk:
                    buf += chunk.decode(errors='replace')

                    if fail_re.search(buf):
                        raise AssertionError("Unity reported FAIL\n" + buf)

                    m = summary_re.search(buf)
                    if m:
                        failures = int(m.group(2))
                        if failures == 0:
                            return buf
                        raise AssertionError("Unity failures detected\n" + buf)

                    if ok_re.search(buf):
                        return buf

                time.sleep(0.05)

            raise AssertionError("Timeout waiting for Unity output\n" + buf)

        finally:
            ser.close()

    # @keyword("Capture Unity Output")
    # def capture_unity_output(self, port, baud, timeout_sec=20):
    #     timeout_sec = float(timeout_sec)
    #     deadline = time.time() + timeout_sec

    #     buf = ""
    #     ser = None

    #     summary_re = re.compile(r'(\d+)\s+Tests\s+(\d+)\s+Failures\s+(\d+)\s+Ignored', re.MULTILINE)
    #     fail_re = re.compile(r'\bFAIL\b')
    #     ok_re = re.compile(r'\bOK\b|\bPASS\b')

    #     try:
    #         ser = serial.Serial(port=port, baudrate=int(baud), timeout=0.2)

    #         while time.time() < deadline:
    #             chunk = ser.read(512)
    #             if chunk:
    #                 buf += chunk.decode('utf-8', errors='replace')

    #                 if fail_re.search(buf):
    #                     raise AssertionError("Unity reported FAIL.\n\n" + buf)

    #                 m = summary_re.search(buf)
    #                 if m:
    #                     tests = int(m.group(1))
    #                     failures = int(m.group(2))
    #                     ignored = int(m.group(3))

    #                     if failures == 0:
    #                         return buf

    #                     raise AssertionError(
    #                         f"Unity reported failures: tests={tests}, failures={failures}, ignored={ignored}\n\n{buf}"
    #                     )

    #                 if ok_re.search(buf) and not fail_re.search(buf):
    #                     return buf

    #             time.sleep(0.05)

    #         raise AssertionError(f"Timed out after {timeout_sec} seconds waiting for Unity output.\n\nPartial log:\n{buf}")

    #     finally:
    #         if ser is not None and ser.is_open:
    #             ser.close()