#define PUGIXML_COMPACT

#include "pugixml-1.11/src/pugixml.hpp"
#include <algorithm>
#include <bits/stdc++.h>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

struct wiki_article_t
{
    int64_t     page_id_;
    std::string title_;
    std::string text_;
    std::string processed_text_;
};

class WikiArticleScanner
{
public:
    WikiArticleScanner(const std::string& documentName);
    wiki_article_t        getFirstArticle();
    void                  analyzeArticles(const wiki_article_t& wikiArticle);
    std::set<std::string> getKeyWordsPerArticle(const wiki_article_t& wikiArticle);
    double                accuracyOfKeyWordPrediction(const std::set<std::string>& predictedSet,
                                                      const std::set<std::string>& actualSet);
    std::set<std::string> getTfIdfKeywords();
    std::set<std::string> getKeyprasenessKeywords();

private:
    void        performTF(const wiki_article_t& wikiArticle);
    void        performIDFPerDocument(const std::string& processedText, const std::string& text);
    void        performTFIDF(int64_t numberOfArticles);
    std::string toLower(std::string& s);

    std::map<std::string, int64_t> m_termFrequencies;
    std::map<std::string, int64_t> m_wordFrequency;
    std::map<std::string, int64_t> m_keywordFrequency;
    std::map<std::string, int64_t> m_tfIdfScoreForWords;
    wiki_article_t                 m_wikiArticle;
    pugi::xml_document             m_xmlDocument;
};