#include "pugixml-1.11/src/pugixml.hpp"
#include <iostream>
#include <sstream>
#include <vector>
#include <memory>
#include <algorithm>
#include <unordered_map>
#include <bits/stdc++.h>
#include <string>

struct wiki_article_t {
    int64_t page_id_;
    std::string title_;
    std::string text_;
    std::string processed_text_;
};

class WikiArticleScanner {
public:
    WikiArticleScanner(const std::string& documentName);
    void parseArticles();
    std::unique_ptr<wiki_article_t> getArticleAt(const int64_t position);
    std::set<std::string> getKeyWordsPerArticle(const wiki_article_t& wikiArticle);
    std::set<std::string> getTfIdfKeywords(const wiki_article_t& wikiArticle);
    double accuracyOfKeyWordPrediction(const std::set<std::string>& predictedSet, const std::set<std::string>& actualSet);
private:
    void performTF();
    void performIDF();
    void performTFIDF();
    std::set<std::string>  getTfIdfKeyWordSet();
    std::string toLower(std::string& s);

    std::map<std::string,int64_t> m_termFrequencies;
    std::map<std::string,int64_t> m_wordFrequencyIDF;
    std::map<std::string,int64_t> m_tfIdfScoreForWords;
    wiki_article_t m_wikiArticle;
    std::vector<std::unique_ptr<wiki_article_t>> m_wikipediaArticles;
    pugi::xml_document m_xmlDocument;
};