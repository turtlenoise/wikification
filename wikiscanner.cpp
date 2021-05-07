#include "wikiscanner.h"

WikiArticleScanner::WikiArticleScanner(const std::string& documentName) {
    pugi::xml_parse_result result = m_xmlDocument.load_file("smalltext.xml");
    if (!result) {
        throw std::invalid_argument("smalltext.xml could not be opened.");
    }
}

std::set<std::string> WikiArticleScanner::getKeyWordsPerArticle(const wiki_article_t& wikiArticle) {
    std::set<std::string> keyWordsPerArticle;
    std::stringstream textStream(wikiArticle.text_);
    std::string wordToken;
    while (getline(textStream, wordToken, ' ')) {
        if (wordToken.rfind("[[",0) == 0) {
            std::string processedWord = wordToken;
            processedWord.erase(remove(processedWord.begin(), processedWord.end(), '['), processedWord.end());
            processedWord.erase(remove(processedWord.begin(), processedWord.end(), ']'), processedWord.end());
            keyWordsPerArticle.insert(toLower(processedWord));
        }
    }
    return keyWordsPerArticle;
}

void WikiArticleScanner::parseArticles() {
    for (pugi::xml_node page: m_xmlDocument.child("mediawiki").children("page"))
    {
        std::string title = page.child("title").child_value();
        int64_t pageID = std::stoi(page.child("id").child_value());
        auto&& revision = page.child("revision");
        std::string text = revision.child("text").child_value();

        if (text.size() >= 1024) {
            std::string processedText = text;
            processedText.erase(remove(processedText.begin(), processedText.end(), '['), processedText.end());
            processedText.erase(remove(processedText.begin(), processedText.end(), ']'), processedText.end());

            wiki_article_t wikiArticle = {
                pageID,
                title,
                text,
                processedText
            };
            m_wikipediaArticles.push_back(std::make_unique<wiki_article_t>(wikiArticle));
        }    
    }
}

std::unique_ptr<wiki_article_t> WikiArticleScanner::getArticleAt(const int64_t position) {
    if (position > m_wikipediaArticles.size() || position < 0) {
        throw std::out_of_range("no element exists at the " + std::to_string(position) + ". position.");
    }
    return std::move(m_wikipediaArticles[position]);
}

template<typename Iterator>
bool alreadyInTheMap(std::pair<Iterator, bool> const& insertionResult)
{
    return !insertionResult.second;
}

std::string WikiArticleScanner::toLower(std::string& s) {
    std::transform(s.begin(), s.end(), s.begin(), 
                   [](unsigned char c){ return std::tolower(c); } 
                  );
    return s;
}

void WikiArticleScanner::performTF() {
    std::stringstream textStream(m_wikiArticle.processed_text_);
    std::string wordToken;

    while (getline(textStream, wordToken, ' ')) {
        if (wordToken.empty()) {
            wordToken = "";
        }
        auto&& insertionResult = m_termFrequencies.insert({toLower(wordToken),1});
        if (alreadyInTheMap(insertionResult)) {
            ++m_termFrequencies[wordToken];
        }
    }
}

void WikiArticleScanner::performIDF() {
    for (std::map<std::string, int64_t>::const_iterator termIter = m_termFrequencies.begin(); termIter != m_termFrequencies.end(); ++termIter) {
        const std::string& word = termIter->first;

        for (std::vector<std::unique_ptr<wiki_article_t>>::const_iterator wikiDocsIter = 
        m_wikipediaArticles.begin(); wikiDocsIter != m_wikipediaArticles.end(); ++wikiDocsIter) {
            if (wikiDocsIter->get() != nullptr) {
                const wiki_article_t& wikiArticle = *wikiDocsIter->get();
                std::string::size_type found = wikiArticle.processed_text_.find(word);
                if (found != std::string::npos) {
                    auto&& insertionResult = m_wordFrequencyIDF.insert({word,1});
                    if (alreadyInTheMap(insertionResult)) {
                        ++m_wordFrequencyIDF[word];
                    }
                } else {
                    m_wordFrequencyIDF.insert({word,0});
                }
            }
        }
    }
}

void WikiArticleScanner::performTFIDF() {
    const int64_t numberOfAllDocuments = m_wikipediaArticles.size();    
    for (auto&& termIter = m_termFrequencies.begin(); termIter != m_termFrequencies.end(); ++termIter) {
        const std::string& word = termIter->first;
        if (m_wordFrequencyIDF[word] > 0) {
            const double tfIdfPerWord = m_termFrequencies[word] * log(numberOfAllDocuments/m_wordFrequencyIDF[word]);
            m_tfIdfScoreForWords.insert({word, tfIdfPerWord});
        } else {
        	// TODO: ?
        	double eps = 1e-6;
            const double tfIdfPerWord = m_termFrequencies[word] * log(numberOfAllDocuments/eps);        	
            m_tfIdfScoreForWords.insert({word, tfIdfPerWord});
        }
    }
}

std::set<std::string>  WikiArticleScanner::getTfIdfKeyWordSet() {
    std::set<std::string> keywords;
    std::multimap<int64_t,std::string> tfIdfScores;
    for (auto&& tfIdfIter = m_tfIdfScoreForWords.begin(); tfIdfIter != m_tfIdfScoreForWords.end(); ++tfIdfIter) {
        tfIdfScores.insert({tfIdfIter->second*1000, tfIdfIter->first});
    }

    const int64_t numberOfKeyWords = m_termFrequencies.size() * 6 / 100;	
	int64_t count = 0;
	std::multimap<int64_t,std::string>::const_iterator bestTfIdfIter = tfIdfScores.end();
    while (count != numberOfKeyWords && bestTfIdfIter != tfIdfScores.begin()) {
    	--bestTfIdfIter;
    	keywords.insert(bestTfIdfIter->second);
    	++count;
    }

    return keywords;
}

std::set<std::string> WikiArticleScanner::getTfIdfKeywords(const wiki_article_t& wikiArticle) {
    m_wikiArticle = wikiArticle;
    performTF();
    performIDF();
    performTFIDF();
    std::set<std::string> keyWords = getTfIdfKeyWordSet();
    return keyWords;
}

double WikiArticleScanner::accuracyOfKeyWordPrediction(const std::set<std::string>& predictedSet, const std::set<std::string>& actualSet) {
    double accuracy = 0;
    double correctPrediction = 0;
    for (auto&& predictedWordIter = predictedSet.begin(); predictedWordIter != predictedSet.end(); ++predictedWordIter) {
        if (actualSet.find(*predictedWordIter) != actualSet.end()) {
            ++correctPrediction;
        }
    }
    // std::cout << "correctPrediction: " << correctPrediction << std::endl;
    // std::cout << "predictedSet.size(): " << predictedSet.size() << std::endl;
    return (correctPrediction/predictedSet.size());
}

int main()
{
    std::unique_ptr<WikiArticleScanner> wikiScanner = std::make_unique<WikiArticleScanner>("smalltext.xml");

    wikiScanner->parseArticles();
    std::unique_ptr<wiki_article_t> wikiArticle = wikiScanner->getArticleAt(0);
    std::cout << wikiArticle->title_ << std::endl;

    std::set<std::string> tfIdfKeywords = wikiScanner->getTfIdfKeywords(*wikiArticle);
    std::cout << "number of tf-idf keywords: " << tfIdfKeywords.size() << std::endl;

    // for (auto&& keyWordIter = tfIdfKeywords.begin(); keyWordIter != tfIdfKeywords.end(); ++keyWordIter) {
    //     std::cout << *keyWordIter << std::endl;
    // }

    std::set<std::string> actualKeywords = wikiScanner->getKeyWordsPerArticle(*wikiArticle);
    std::cout << "number of actual keywords: " << actualKeywords.size() << std::endl;

    // for (auto&& keywordIter = actualKeywords.begin(); keywordIter != actualKeywords.end(); ++keywordIter) {
    //     std::cout << *keywordIter << std::endl;
    // }
    double tfIdfAccuracy = wikiScanner->accuracyOfKeyWordPrediction(tfIdfKeywords,actualKeywords);

    std::cout << "accuracy using tf-idf: " << tfIdfAccuracy << std::endl;

    return 0;
}
