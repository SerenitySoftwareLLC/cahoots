"""
The MIT License (MIT)

Copyright (c) Serenity Software, LLC

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
"""
from cahoots.parsers.base import BaseParser
from cahoots.parsers.programming.lexer import ProgrammingLexer
from cahoots.parsers.programming.bayesian import ProgrammingBayesianClassifier
from SereneRegistry import registry
import os
import re
import glob
import yaml


class ProgrammingParser(BaseParser):
    """Determines if a sample of text is a programming language"""

    all_keywords = []
    language_keywords = {}

    @staticmethod
    def bootstrap(config):
        """Loads tokens from the yaml files on disk"""
        all_keywords = []
        language_keywords = {}

        directory = os.path.dirname(os.path.abspath(__file__))
        path = os.path.join(directory, "languages/*.yaml")

        for file_path in glob.glob(path):
            with open(file_path, 'r') as language_file:
                language = yaml.load(language_file)
                all_keywords.extend(language['keywords'])
                language_keywords[language['id']] = language

        registry.set('PP_all_keywords', set(all_keywords))
        registry.set('PP_language_keywords', language_keywords)

        ProgrammingBayesianClassifier.bootstrap(config)

    def __init__(self, config):
        BaseParser.__init__(self, config, "Programming", 0)
        self.all_keywords = registry.get('PP_all_keywords')
        self.language_keywords = registry.get('PP_language_keywords')

    # finding common words/phrases in programming languages
    def find_common_tokens(self, dataset):
        """Looking for any programming language keywords in the data"""
        return [keyword for keyword in dataset if keyword in self.all_keywords]

    @classmethod
    def basic_language_heuristic(cls, language_data, dataset):
        """Looking for specific programming language keywords in the data"""
        return [keyword for keyword in
                dataset if keyword in language_data['keywords']]

    @classmethod
    def create_dataset(cls, data):
        """Takes a data string and converts it into a dataset for analysis"""
        return set(re.split('[ ;,{}()\n\t\r]', data.lower()))

    def calculate_confidence(self, lex_languages, bayes_languages):
        """Normalizing the parsing scores generated by lex and bayes parsing"""

        # Adding a little bit of confidence for each lexed language we find.
        lex_scores = {}
        for lang, lex_score in lex_languages.items():
            lex_scores[lang] = lex_score / 100.0

        # Removing any scores that are less than 1% confidence
        scores = {}
        max_score = 0.0
        for lang, lex_score in lex_scores.items():
            scorecard = {
                'scores': {
                    'lexer': lex_score,
                    'bayes': bayes_languages.get(lang, 0.0)
                },
                'confidence': None
            }
            scorecard['scores']['final'] = \
                scorecard['scores']['lexer'] + scorecard['scores']['bayes']
            if scorecard['scores']['final'] > 1:
                max_score = max(max_score, scorecard['scores']['final'])
                # Rounding the result since it's a confidence value
                scores[lang] = scorecard

        # Making sure all scores are normalized to under 100
        scores = self.normalize_scores(max_score, scores)

        scores = self.prepare_scores(scores)

        return scores

    @classmethod
    def normalize_scores(cls, max_score, scores):
        """
        If the classifier scores reach over 100
        we normalize all the scores appropriately
        """
        if max_score <= 100:
            return scores

        normal_scores = {}
        modifier = 100.0/max_score
        for lang, scorecard in scores.items():
            normal_scores[lang] = scorecard
            normal_scores[lang]['scores']['lexer'] = \
                modifier * scorecard['scores']['lexer']
            normal_scores[lang]['scores']['bayes'] = \
                modifier * scorecard['scores']['bayes']
            normal_scores[lang]['scores']['final'] = \
                modifier * scorecard['scores']['final']

        return normal_scores

    @classmethod
    def prepare_scores(cls, scores):
        """cleans up score values and generates integer confidence value"""
        for _, scorecard in scores.items():
            scorecard['confidence'] = int(round(scorecard['scores']['final']))
            scorecard['scores']['lexer'] = \
                round(scorecard['scores']['lexer'], 3)
            scorecard['scores']['bayes'] = \
                round(scorecard['scores']['bayes'], 3)
            scorecard['scores']['final'] = \
                round(scorecard['scores']['final'], 3)

        return scores

    def get_possible_languages(self, dataset):
        """
        Goes through each language's keywords and finds
        languages that share keywords with the provided data set
        """
        return [language for language, language_data in
                self.language_keywords.items() if
                self.basic_language_heuristic(
                    language_data, dataset
                )]

    def parse(self, data):
        """
        Determines if the data is an example of one of our trained languages
        """
        dataset = self.create_dataset(data)

        # Step 1: Is this possibly code?
        if not self.find_common_tokens(dataset):
            return

        # Step 2: Which languages match, based on keywords alone?
        matched_languages = self.get_possible_languages(dataset)

        # Step 3: Which languages match, based on a smarter lexer?
        lexer = ProgrammingLexer(matched_languages, data.lower())
        lex_languages = lexer.lex()

        if not lex_languages:
            return

        # Step 4: Using a Naive Bayes Classifier
        # to pinpoint the best language fits
        classifier = ProgrammingBayesianClassifier()
        bayes_languages = classifier.classify(data)

        scores = self.calculate_confidence(lex_languages, bayes_languages)

        for lang_id, scorecard in scores.items():
            yield self.result(
                self.language_keywords[lang_id]['name'],
                scorecard['confidence'],
                scorecard
            )
