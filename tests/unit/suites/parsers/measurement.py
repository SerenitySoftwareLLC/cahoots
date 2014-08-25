from cahoots.parsers.measurement import MeasurementParser
from tests.unit.config import TestConfig
from SereneRegistry import registry
import unittest


class MeasurementParserTests(unittest.TestCase):
    """Unit Testing of the MeasurementParser"""

    mp = None

    def setUp(self):
        MeasurementParser.bootstrap(TestConfig())
        self.mp = MeasurementParser(TestConfig())

    def tearDown(self):
        self.mp = None

    def test_loadUnits(self):
        self.assertTrue(registry.test('MPallUnits'))
        self.assertTrue(registry.test('MPsystemUnits'))

    def test_basicUnitCheck(self):
        self.assertTrue(self.mp.basicUnitCheck('parsec'))
        self.assertFalse(self.mp.basicUnitCheck('heffalump'))

    def test_determineSystemUnit(self):
        self.assertEqual(
            self.mp.determineSystemUnit('inches'),
            'imperial_length'
        )
        self.assertEqual(
            self.mp.determineSystemUnit('parsecs'),
            'miscellaneous_length'
        )
        self.assertIsNone(self.mp.determineSystemUnit('asdfasdf'))

    def test_identifyUnitsInData(self):
        data, matches = self.mp.identifyUnitsInData('3 square feet')
        self.assertEqual(data, '3')
        self.assertEqual(matches, ['square feet'])

        self.assertRaises(
            Exception,
            self.mp.identifyUnitsInData,
            '3 feet feet'
        )

    def test_getSubType(self):
        self.assertEqual(
            self.mp.getSubType('imperial_length'),
            'Imperial Length'
        )
        self.assertEqual(
            self.mp.getSubType('miscellaneous_length'),
            'Miscellaneous Length'
        )