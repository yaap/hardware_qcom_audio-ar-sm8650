#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 The LineageOS Project
# SPDX-License-Identifier: Apache-2.0

import sys
sys.dont_write_bytecode = True

from pathlib import Path
import tempfile
import unittest

from generate_oprec_profiles import generate


class GenerateProfilesTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)

    def profile(self, name, content=b""):
        path = self.root / name
        path.write_bytes(content)
        return path

    def test_id_comes_from_filename(self):
        self.profile("oprec_record_ID4_CamBack.bin")
        header = generate(self.root)
        self.assertIn("CAMBACK = 4,", header)

    def test_bin_and_txt_share_one_constant(self):
        self.profile("oprec_record_ID1_MIC_normal.bin")
        self.profile("oprec_record_ID1_MIC_normal.txt")
        header = generate(self.root)
        self.assertEqual(header.count("MIC_NORMAL = 1,"), 1)

    def test_calibration_files_are_ignored(self):
        self.profile("oprec_camback_calibration.bin")
        self.profile("oprec_record_ID9_HE.bin")
        header = generate(self.root)
        self.assertNotIn("CALIBRATION", header)
        self.assertIn("HE = 9,", header)

    def test_duplicate_id_with_different_names_is_rejected(self):
        self.profile("oprec_record_ID4_CamBack.bin")
        self.profile("oprec_record_ID4_HE_Usb.bin")
        with self.assertRaisesRegex(ValueError, "profile ID 4 names both"):
            generate(self.root)

    def test_empty_directory_is_rejected(self):
        with self.assertRaisesRegex(ValueError, "no profile entries"):
            generate(self.root)

    def test_vendor_files_are_not_modified(self):
        path = self.profile("oprec_record_ID2_MIC_App1.txt", b"vendor data")
        before = path.read_bytes()
        generate(self.root)
        self.assertEqual(path.read_bytes(), before)

    def test_output_is_deterministic(self):
        self.profile("oprec_record_ID5_MIC_Conference.bin")
        self.profile("oprec_record_ID1_MIC_normal.bin")
        self.assertEqual(generate(self.root), generate(self.root))


if __name__ == "__main__":
    unittest.main()
