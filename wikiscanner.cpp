#include "wikiscanner.h"

WikiArticleScanner::WikiArticleScanner(const std::string& documentName)
{
    pugi::xml_parse_result result = m_xmlDocument.load_file("enwiki.xml");
    if (!result)
    {
        throw std::invalid_argument("enwiki.xml could not be opened.");
    }
}

std::set<std::string> WikiArticleScanner::getKeyWordsPerArticle(const wiki_article_t& wikiArticle)
{
    std::set<std::string> keyWordsPerArticle;
    std::stringstream     textStream(wikiArticle.text_);
    std::string           wordToken;
    while (getline(textStream, wordToken, ' '))
    {
        if (wordToken.rfind("[[", 0) == 0)
        {
            std::string processedWord = wordToken;
            processedWord.erase(remove(processedWord.begin(), processedWord.end(), '['), processedWord.end());
            processedWord.erase(remove(processedWord.begin(), processedWord.end(), ']'), processedWord.end());
            keyWordsPerArticle.insert(processedWord);
        }
    }
    return keyWordsPerArticle;
}

std::string WikiArticleScanner::toLower(std::string& s)
{
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
    return s;
}

wiki_article_t WikiArticleScanner::getFirstArticle()
{
    for (pugi::xml_node page : m_xmlDocument.child("mediawiki").children("page"))
    {
        std::string title    = page.child("title").child_value();
        int64_t     pageID   = std::stoi(page.child("id").child_value());
        auto&&      revision = page.child("revision");
        std::string text     = revision.child("text").child_value();

        if (text.size() >= 1024)
        {
            std::string processedText = text;
            processedText.erase(remove(processedText.begin(), processedText.end(), '['), processedText.end());
            processedText.erase(remove(processedText.begin(), processedText.end(), ']'), processedText.end());

            wiki_article_t wikiArticle = {pageID, title, toLower(text), toLower(processedText)};
            return wikiArticle;
        }
    }
}

template <typename Iterator>
bool alreadyInTheMap(std::pair<Iterator, bool> const& insertionResult)
{
    return !insertionResult.second;
}

void WikiArticleScanner::performTF(const wiki_article_t& wikiArticle)
{
    std::stringstream textStream(wikiArticle.processed_text_);
    std::string       wordToken;

    while (getline(textStream, wordToken, ' '))
    {
        if (wordToken.empty())
        {
            wordToken = "";
        }
        auto&& insertionResult = m_termFrequencies.insert({wordToken, 1});
        if (alreadyInTheMap(insertionResult))
        {
            ++m_termFrequencies[wordToken];
        }
    }
}

void WikiArticleScanner::performIDFPerDocument(const std::string& processedText, const std::string& text)
{
    for (std::map<std::string, int64_t>::const_iterator termIter = m_termFrequencies.begin();
         termIter != m_termFrequencies.end(); ++termIter)
    {
        const std::string& word = termIter->first;

        std::string::size_type found = processedText.find(word);
        if (found != std::string::npos)
        {
            auto&& insertionResult = m_wordFrequency.insert({word, 1});
            if (alreadyInTheMap(insertionResult))
            {
                ++m_wordFrequency[word];
            }
        }
        else
        {
            m_wordFrequency.insert({word, 0});
        }

        std::string::size_type foundKeyWord = text.find("[[" + word + "]]");
        if (foundKeyWord != std::string::npos)
        {
            auto&& insertionResult = m_keywordFrequency.insert({word, 1});
            if (alreadyInTheMap(insertionResult))
            {
                ++m_keywordFrequency[word];
            }
        }
        else
        {
            m_keywordFrequency.insert({word, 0});
        }
    }
}

void WikiArticleScanner::performTFIDF(int64_t numberOfArticles)
{
    for (auto&& termIter = m_termFrequencies.begin(); termIter != m_termFrequencies.end(); ++termIter)
    {
        const std::string& word = termIter->first;
        if (m_wordFrequency[word] > 0)
        {
            const double tfIdfPerWord = m_termFrequencies[word] * log(numberOfArticles / m_wordFrequency[word]);
            m_tfIdfScoreForWords.insert({word, tfIdfPerWord});
        }
        else
        {
            // TODO: ?
            double       eps          = 1e-6;
            const double tfIdfPerWord = m_termFrequencies[word] * log(numberOfArticles / eps);
            m_tfIdfScoreForWords.insert({word, tfIdfPerWord});
        }
    }
}

std::set<std::string> WikiArticleScanner::getTfIdfKeywords()
{
    std::set<std::string>               keywords;
    std::multimap<int64_t, std::string> tfIdfScores;
    for (auto&& tfIdfIter = m_tfIdfScoreForWords.begin(); tfIdfIter != m_tfIdfScoreForWords.end(); ++tfIdfIter)
    {
        tfIdfScores.insert({tfIdfIter->second * 1000, tfIdfIter->first});
    }

    const int64_t                                       numberOfKeyWords = m_termFrequencies.size() * 6 / 100;
    int64_t                                             count            = 0;
    std::multimap<int64_t, std::string>::const_iterator bestTfIdfIter    = tfIdfScores.end();
    while (count != numberOfKeyWords && bestTfIdfIter != tfIdfScores.begin())
    {
        --bestTfIdfIter;
        keywords.insert(bestTfIdfIter->second);
        ++count;
    }

    return keywords;
}

double WikiArticleScanner::accuracyOfKeyWordPrediction(const std::set<std::string>& predictedSet,
                                                       const std::set<std::string>& actualSet)
{
    double accuracy          = 0;
    double correctPrediction = 0;
    for (auto&& predictedWordIter = predictedSet.begin(); predictedWordIter != predictedSet.end(); ++predictedWordIter)
    {
        if (actualSet.find(*predictedWordIter) != actualSet.end())
        {
            ++correctPrediction;
        }
    }
    return (correctPrediction / predictedSet.size());
}

std::set<std::string> WikiArticleScanner::getKeyprasenessKeywords()
{
    std::set<std::string>          keywordSet;
    std::map<std::string, int64_t> keyPhrasenessMap;
    for (auto&& wordIter = m_wordFrequency.begin(); wordIter != m_wordFrequency.end(); ++wordIter)
    {
        int64_t keyprasenessProbability = 0;
        if (m_wordFrequency[wordIter->first] > 0)
        {
            keyprasenessProbability = m_keywordFrequency[wordIter->first] / m_wordFrequency[wordIter->first];
        }
        else
        {
            double eps              = 1e-6;
            keyprasenessProbability = m_keywordFrequency[wordIter->first] / eps;
        }
        keyPhrasenessMap.insert({wordIter->first, keyprasenessProbability});
    }

    std::multimap<int64_t, std::string> keyphrasenessScores;
    for (auto&& keyphrasenessIter = keyPhrasenessMap.begin(); keyphrasenessIter != keyPhrasenessMap.end();
         ++keyphrasenessIter)
    {
        keyphrasenessScores.insert({keyphrasenessIter->second * 1000, keyphrasenessIter->first});
    }

    const int64_t                                       numberOfKeyWords      = m_termFrequencies.size() * 6 / 100;
    int64_t                                             count                 = 0;
    std::multimap<int64_t, std::string>::const_iterator bestKeyphrasenessIter = keyphrasenessScores.end();
    while (count != numberOfKeyWords && bestKeyphrasenessIter != keyphrasenessScores.begin())
    {
        --bestKeyphrasenessIter;
        keywordSet.insert(bestKeyphrasenessIter->second);
        ++count;
    }
    return keywordSet;
}

void WikiArticleScanner::analyzeArticles(const wiki_article_t& wikiArticle)
{
    int64_t numberOfArticles = 0;
    performTF(wikiArticle);
    for (pugi::xml_node page : m_xmlDocument.child("mediawiki").children("page"))
    {
        auto&&      revision = page.child("revision");
        std::string rawText  = revision.child("text").child_value();

        if (rawText.size() >= 1024)
        {
            std::string processedText = toLower(rawText);
            processedText.erase(remove(processedText.begin(), processedText.end(), '['), processedText.end());
            processedText.erase(remove(processedText.begin(), processedText.end(), ']'), processedText.end());
            std::string text = toLower(rawText);
            performIDFPerDocument(processedText, text);
            ++numberOfArticles;
            if (numberOfArticles == 2000)
            {
                break;
            }
        }
    }
    performTFIDF(numberOfArticles);
}

int main()
{
    std::unique_ptr<WikiArticleScanner> wikiScanner = std::make_unique<WikiArticleScanner>("smalltext.xml");

    wiki_article_t firstWikiArticle = wikiScanner->getFirstArticle();
    std::cout << "title: " << firstWikiArticle.title_ << std::endl;

    wikiScanner->analyzeArticles(firstWikiArticle);

    std::set<std::string> actualKeywords = wikiScanner->getKeyWordsPerArticle(firstWikiArticle);
    std::cout << "number of actual keywords: " << actualKeywords.size() << std::endl;

    std::set<std::string> tfIdfKeywords = wikiScanner->getTfIdfKeywords();
    std::cout << "number of keywords (6%): " << tfIdfKeywords.size() << std::endl;
    double tfIdfAccuracy = wikiScanner->accuracyOfKeyWordPrediction(tfIdfKeywords, actualKeywords);
    std::cout << "accuracy using tf-idf: " << tfIdfAccuracy << std::endl;

    std::set<std::string> keyprasenessKeywords = wikiScanner->getKeyprasenessKeywords();
    double keyprasenessAccuracy = wikiScanner->accuracyOfKeyWordPrediction(keyprasenessKeywords, actualKeywords);
    std::cout << "accuracy using keyphraseness: " << keyprasenessAccuracy << std::endl;

    return 0;
}
