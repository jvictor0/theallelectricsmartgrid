"""Regression checks for budgets, whole-curve scoring and post-hoc weighting."""
from fractions import Fraction
from pathlib import Path
import ctypes as ct
import subprocess
import tempfile
import unittest
from unittest.mock import patch

import numpy as np

from experiment import Design, F64, HERE, ROOT, visible_rms
from run import GENERATED, digest, export, load_prepared, packed_table, query, write_json


class SurveyTests(unittest.TestCase):
    def setUp(self):
        scratch = ROOT/"build/response-survey/tests"
        scratch.mkdir(parents=True, exist_ok=True)
        self.temp = tempfile.TemporaryDirectory(dir=scratch)
        self.addCleanup(self.temp.cleanup)
        self.out = Path(self.temp.name)
        self.gen = self.out/"generated"
        self.gen.mkdir()
        self.recipe = dict(axes=[dict(name="x",low=1,high=16,knots=5,cells=2,logScale=True,points=False)],
                           stimuli=[dict(name=s,mass=1,prelude=0,preludeRms=0,rms=0.25) for s in ("A","B")],
                           masses=[1,0,0,3], frequencies=[1,2,4], sampleRate=16, bandLimitHz=0,
                           records=1, frames=1, audit=4, warm=0, training=1, fft=16, floorDb=-60,
                           budgetBytes=80, thresholdsDb=[0.5], coveragePPM=[500000], riskPPM=50000)
        write_json(self.gen/"recipe.json",self.recipe)
        write_json(self.gen/"processor.json",dict(axes=["x"],stateSize=1))

    def test_missing_export_cannot_reuse_old_kernel(self):
        for name in GENERATED:
            (self.gen/name).write_text("previous export")
        with patch("run.subprocess.run"), self.assertRaisesRegex(ValueError, "#survey_processor"):
            export(self.out/"NoProcessor.lean", self.gen)
        self.assertFalse(any((self.gen/name).exists() for name in GENERATED))

    def test_staged_export_matches_plan(self):
        work = self.out/"work"
        work.mkdir()
        for name in GENERATED:
            (work/name).write_text(name)
        record = dict(sourceSHA256="source", files={n:digest(work/n) for n in GENERATED})
        write_json(self.out/"export.json",record)
        self.assertEqual(load_prepared(self.out,self.gen),record)
        self.assertTrue(all(digest(self.gen/n)==record["files"][n] for n in GENERATED))
        (work/"processor.metal").write_text("different processor")
        with self.assertRaisesRegex(ValueError,"differs from plan"):
            load_prepared(self.out,self.gen)

    def test_byte_budget_and_sampling(self):
        d = Design(self.gen)
        self.assertEqual(d.storage_shape,[2])
        self.assertEqual(d.active,[0,3])
        raw = np.array([[-20.,-30,-40],[-18,-25,-35],[-10,-20,-30],[-5,-15,-25],[0,-10,-20]])
        table = packed_table(d,raw)
        self.assertEqual(len(table),80)
        np.testing.assert_array_equal(np.frombuffer(table[-12:],dtype="<i2").reshape(2,3),64*raw[[0,4]])
        rng = np.random.default_rng(10)
        for _ in range(30):
            config, stimulus = d.draw(3,rng)
            self.assertLessEqual(4,config[0])
            self.assertLessEqual(config[0],16)
            self.assertEqual(stimulus,1)
        write_json(self.gen/"recipe.json",dict(self.recipe,budgetBytes=79))
        with self.assertRaises(ValueError):
            Design(self.gen)

    def test_floor_crossing(self):
        # First half of the log interval is invisible; the rest is a triangle.
        a = np.array([-70., -50.])
        b = np.array([-66., -66.])
        self.assertAlmostEqual(visible_rms(a,b,np.array([20.,200.]),-60),np.sqrt(100/6))

    def test_nearby_knots_with_equal_rounded_logs(self):
        f = np.array([10000., 10000.000000000004])
        self.assertAlmostEqual(visible_rms(np.zeros(2), np.full(2, 10.), f, -60), 10.)

    def test_nonfinite_response_is_a_miss(self):
        self.assertIsNone(visible_rms(np.zeros(2), np.full(2, np.nan), np.array([20., 200.]), -60))

    def test_frequency_alignment(self):
        recipe = dict(self.recipe, frequencies=[1,2,4,8,16], sampleRate=32,
                      budgetBytes=1000, floorDb=-22)
        recipe["axes"] = [dict(recipe["axes"][0], low=2, high=8, knots=2)]
        def design(**changes):
            write_json(self.gen/"recipe.json", dict(recipe, **changes))
            return Design(self.gen)
        with self.assertRaisesRegex(ValueError, "positive axis"):
            design(frequencyRelativeTo="missing")

        driver = self.out/"display.cpp"
        driver.write_text('#include "Table.cpp"\n'
                          'extern "C" double Display(const void* p, size_t size, double x, double hz)\n'
                          '{\n    return dsp_survey::ResponseTable(p, size).DisplayDb(&x, hz);\n}\n')
        lib = self.out/"display.dylib"
        subprocess.run(["clang++", "-std=c++20", "-shared", "-fPIC", "-I", str(HERE),
                        str(driver), "-o", str(lib)], check=True)
        native = ct.CDLL(str(lib))
        native.Display.argtypes = [ct.c_char_p, ct.c_size_t, ct.c_double, ct.c_double]
        native.Display.restype = ct.c_double
        native.SurveyKnots.argtypes = [ct.c_char_p, ct.c_size_t, F64, ct.c_uint, F64, ct.c_uint]
        native.SurveyKnots.restype = ct.c_bool
        def knots(table, x):
            result = np.empty(5)
            self.assertTrue(native.SurveyKnots(table, len(table), np.array([float(x)]), 1, result, 5))
            return result

        # The peak moves from 2 Hz to 8 Hz. At the middle cutoff it belongs at 4 Hz.
        raw = np.array([[-20,0,-20,-30,-40], [-40,-30,-20,0,-20]], dtype=float)
        fixed = packed_table(design(), raw)
        relative = packed_table(design(frequencyRelativeTo="x"), raw)
        self.assertEqual(fixed[:4], b"DSR1")
        self.assertEqual(relative[:4], b"DSR2")
        self.assertEqual(knots(fixed, 4)[2], -20)
        np.testing.assert_allclose(knots(relative, 4), [-25,-20,0,-20,-25], atol=1e-12)
        self.assertAlmostEqual(native.Display(relative, len(relative), 4, np.sqrt(8)), -10)
        self.assertEqual(native.Display(relative, len(relative), 4, .5), -22)

        # The byte limit puts a packed vertex between two measured cutoffs.
        recipe["axes"][0]["knots"] = 4
        raw = raw[[0,0,1,1]]
        fine = packed_table(design(frequencyRelativeTo="x"), raw)
        coarse = packed_table(design(frequencyRelativeTo="x", budgetBytes=114), raw)
        self.assertEqual(len(coarse), 114)
        np.testing.assert_allclose(knots(coarse, 4), knots(fine, 4), atol=1/128)

    def test_quantized_spectral_mesh(self):
        axes = [dict(self.recipe["axes"][0], name=name, knots=n, low=0, high=1, logScale=False)
                for name,n in zip(("x","y","z"),(5,6,8))]
        recipe = dict(self.recipe, axes=axes, masses=[], spectralMesh=[3,4,5], budgetBytes=2000)
        write_json(self.gen/"recipe.json",recipe)
        write_json(self.gen/"processor.json",dict(axes=["x","y","z"],stateSize=1))
        design = Design(self.gen)
        def spectrum(x,y,z):
            return np.array([-15+7*x+4*y*z, -18-3*x*y+2*z, -5+5*y+x*z])
        curves = np.empty(design.knots+(3,))
        for indices in np.ndindex(design.knots):
            curves[indices] = spectrum(*(np.array(indices)/(np.array(design.knots)-1)))
        artifact = packed_table(design, curves)
        self.assertEqual(len(artifact), design.nbytes)
        self.assertLessEqual(len(artifact), recipe["budgetBytes"])
        path = self.out/"knots.dylib"
        subprocess.run(["clang++","-std=c++20","-shared","-fPIC",str(HERE/"Table.cpp"),"-o",str(path)],check=True)
        reader = ct.CDLL(str(path)).SurveyKnots
        reader.argtypes = [ct.c_char_p,ct.c_size_t,F64,ct.c_uint,F64,ct.c_uint]
        reader.restype = ct.c_bool
        result = np.empty(3)
        for cfg in ([0.,0.,0.],[1.,1.,1.],[.137,.591,.324]):
            self.assertTrue(reader(artifact,len(artifact),np.array(cfg),3,result,3))
            np.testing.assert_allclose(result,spectrum(*cfg),atol=.025)
        self.assertFalse(reader(artifact,len(artifact)-1,np.ones(3),3,result,3))

    def test_independent_and_shared_spectra(self):
        edges = np.array([20,320,640,20000])
        recipe = dict(self.recipe,fft=4096,frames=32,warm=0,sampleRate=192000,bandLimitHz=24000,
                      frequencies=np.sqrt(edges[:-1]*edges[1:]).tolist(), budgetBytes=1000,
                      dualRate=dict(edges=edges.tolist(),splitHz=640,trainingTile=2))
        write_json(self.gen/"recipe.json",recipe)
        design = Design(self.gen)
        class Gain:
            gpu = None
            @staticmethod
            def process(configs, records, x):
                return x*configs[0][0]
        configs = [np.array([.5]), np.array([2.])]
        def measure(points, seeds):
            return design.measure_batch(Gain(),points,0,[np.random.default_rng(seed) for seed in seeds])
        shared = measure(configs,[82])
        independent = measure(configs,[82,93])
        np.testing.assert_array_equal(shared[0,1],shared[1,1])
        self.assertFalse(np.array_equal(independent[0,1],independent[1,1]))
        np.testing.assert_array_equal(independent[1],measure(configs[1:],[93])[0])
        for i, gain in enumerate((.5,2.)):
            np.testing.assert_allclose(independent[i,0]/independent[i,1],gain**2,rtol=1e-5)
            np.testing.assert_allclose(shared[i,0]/shared[i,1],gain**2,rtol=1e-5)

    def test_engine_spectrum_resolves_ultrasonic_bands(self):
        from spectrum import DualRate, EPOCH
        edges = np.array([23.4375, 640, 24000, 48000, 72000, 96000])
        recipe = dict(self.recipe, fft=4096, frames=32, sampleRate=192000, bandLimitHz=96000,
                      dualRate=dict(edges=edges, splitHz=640, trainingTile=2))
        centers = np.sqrt(edges[:-1]*edges[1:])
        with self.assertRaisesRegex(ValueError, "full-band stimulus"):
            DualRate(dict(recipe, bandLimitHz=24000), centers)
        analyzer = DualRate(recipe, centers)
        t = np.arange(EPOCH)/analyzer.rate
        for tone, band in ((36000, 2), (60000, 3), (84000, 4)):
            powers = analyzer.reference(np.sin(2*np.pi*tone*t), 0)
            self.assertAlmostEqual(powers[band], .5, places=6)
            self.assertLess(np.delete(powers, band).max(), 1e-6)
        noise = analyzer.prepare_noise([np.random.default_rng(541)],dict(prelude=0, rms=.25))
        band_density = analyzer.reference(noise[:,0].reshape(-1), 0)/np.diff(edges)
        np.testing.assert_allclose(band_density[2:], .25**2/96000, rtol=.08)

    def test_mass_weighted_region(self):
        rows = [dict(cell=0, indices=[0,0], mass=1, scores=[0]*4, hits=[4], lowerPPM=[500000]),
                dict(cell=3, indices=[1,1], mass=3, scores=[1]*4, hits=[0], lowerPPM=[0])]
        write_json(self.out/"report.json",dict(thresholdsDb=[0.5],riskPPM=50000,cells=rows))
        table = self.out/"table.bin"
        table.write_bytes(b"certified table")
        write_json(self.out/"hashes.json",{s:digest(self.out/s) for s in ("report.json","generated/recipe.json","table.bin")})
        result = query(self.out,None,[],None)
        self.assertEqual(result["tableSHA256"], digest(table))
        self.assertEqual(result["bounds"][0]["estimatedCoverage"],0.25)
        self.assertEqual(Fraction(result["bounds"][0]["lowerCoverage"]),Fraction(1,8))
        self.assertIsNone(result["quantileUpperDbByPPM"]["500000"])
        self.assertEqual(query(self.out,None,["x=0:1"],"A")["quantileUpperDbByPPM"]["500000"],0.5)
        with self.assertRaises(ValueError):
            query(self.out,None,["x=0:1"],"B")
        table.write_bytes(b"replacement table")
        with self.assertRaisesRegex(ValueError, "Published artifact hash differs: table.bin"):
            query(self.out,None,[],None)


if __name__ == "__main__":
    unittest.main()
