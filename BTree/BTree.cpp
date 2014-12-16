#include "BTree.hpp"
#include <vector>
#include "Utils/writeFile.hpp"
#include "Utils/bufOp.hpp"

namespace BTree {
    static const int p1 = 98765431, p2 = 10016957, p3 = 57885161;
    Key::Key(const std::string& _key) {
        hash1 = hash2 = hash3 = 0;
        for (auto c : _key) {
            hash1 = hash1 * p1 + c;
            hash2 = hash2 * p2 + c;
            hash3 = hash3 * p3 + c;
        }
    }
    Key::Key(int _key) {
        hash1 = hash2 = hash3 = _key;
    }
    Key::Key(int _hash1, int _hash2, int _hash3) :
        hash1(_hash1), hash2(_hash2), hash3(_hash3) {}
    const int MinChild = 255;
    struct Node {
        int size;
        struct Child {
            Key less;
            PageDB::Location loc;
        } children[MinChild * 2];
        int findIndex(const Key& key) const {
            int sb = 0, se = size - 1;
            while (sb < se) {
                int sm = (sb + se) / 2 + 1;
                if (key < children[sm].less)
                    se = sm - 1;
                else
                    sb = sm;
            }
            return sb;
        }
        bool InsertAndSplit(Key key, PageDB::Location loc, Node& splitNode) {
            if (size == MinChild * 2) {
                size = MinChild;
                splitNode.size = MinChild;
                for (int i = 0; i < MinChild; i++)
                    splitNode.children[i] = children[i + MinChild];
                if (key < splitNode.children[0].less)
                    InsertAndSplit(key, loc, splitNode);
                else
                    splitNode.InsertAndSplit(key, loc, splitNode);
                return true;
            }
            int i;
            for (i = size; i > 0; i--) {
                if (key < children[i - 1].less)
                    children[i] = children[i - 1];
                else
                    break;
            }
            children[i].less = key;
            children[i].loc = loc;
            size++;
            return false;
        }
        void ReadFromBuf(const char* buf) {
            size = Utils::readInt(buf);
            for (int i = 0; i < size; i++) {
                children[i].less.hash1 = Utils::readInt(buf);
                children[i].less.hash2 = Utils::readInt(buf);
                children[i].less.hash3 = Utils::readInt(buf);
                children[i].loc.Page = Utils::readWord(buf);
                children[i].loc.Offset = Utils::readWord(buf);
            }
        }
        void WriteToBuf(char* buf) {
            Utils::writeInt(buf, size);
            for (int i = 0; i < size; i++) {
                Utils::writeInt(buf, children[i].less.hash1);
                Utils::writeInt(buf, children[i].less.hash2);
                Utils::writeInt(buf, children[i].less.hash3);
                Utils::writeWord(buf, children[i].loc.Page);
                Utils::writeWord(buf, children[i].loc.Offset);
            }
        }
    };
    const int InformationLength = 16;
    struct Information {
        Key key;
        Value value;
        void ReadFromBuf(const char* buf) {
            key.hash1 = Utils::readInt(buf);
            key.hash2 = Utils::readInt(buf);
            key.hash3 = Utils::readInt(buf);
            value.Page = Utils::readWord(buf);
            value.Offset = Utils::readWord(buf);
        }
        void WriteToBuf(char* buf) {
            Utils::writeInt(buf, key.hash1);
            Utils::writeInt(buf, key.hash2);
            Utils::writeInt(buf, key.hash3);
            Utils::writeWord(buf, value.Page);
            Utils::writeWord(buf, value.Offset);
        }
    };



    BTree::BTree(PageDB::Scheduler* _pgdb, const std::string& fn)
        : pgdb(_pgdb), file(pgdb->OpenFile(fn)),
        entrySession(pgdb->GetWriteSession(file, file->entryPageID)) {
        if (magicNumber() != MagicNumber) {
            initBTree();
        }
    }
    std::pair<bool, Value> BTree::find(const Key& key) {
        std::pair<bool, Value> ret;
        bool found;
        PageDB::Location loc;
        trace(key, nullptr, found, loc);
        if (found) {
            PageDB::PageSession session = pgdb->GetSession(file, loc.Page);
            Information x;
            x.ReadFromBuf(session.buf() + loc.Offset);
            ret.second = x.value;
        }
        return ret;
    }
    bool BTree::set(const Key& key, Value value, bool force) {
        std::vector<int> tr;
        bool found;
        PageDB::Location loc;
        trace(key, &tr, found, loc);
        if (!found && !force)
            return false;
        Information info;
        info.key = key;
        info.value = value;
        if (found) {
            PageDB::PageWriteSession session = pgdb->GetWriteSession(file, loc.Page);
            info.WriteToBuf(session.buf() + loc.Offset);
            return true;
        }
        char buf[InformationLength];
        info.WriteToBuf(buf);
        auto locX = Utils::writeFile(pgdb, file, buf, InformationLength);
        insertCore(key, tr, locX);
        return true;
    }
    bool BTree::remove(const Key& key) {
        std::vector<int> tr;
        bool found;
        PageDB::Location loc;
        trace(key, &tr, found, loc);
        if (!found)
            return false;
        removeCore(key, tr);
        return true;
    }
    void BTree::trace(const Key& key, std::vector<int>* tr, bool& found, PageDB::Location& loc) {
        Node* node = new Node;
        int currentPage = rootPage();
        while (true) {
            if (tr)
                tr->push_back(currentPage);
            PageDB::PageSession session = pgdb->GetSession(file, currentPage);
            node->ReadFromBuf(session.buf());
            if (node->size == 0) {
                found = false;
                break;
            }
            int idx = node->findIndex(key);
            auto locX = node->children[idx].loc;
            if (locX.Page != 0) {
                if (node->children[idx].less == key) {
                    found = true;
                    loc = locX;
                    break;
                } else {
                    found = false;
                    break;
                }
            } else {
                currentPage = locX.Offset;
            }
        }
        delete node;
    }
    void BTree::initBTree() {
        magicNumber() = MagicNumber;
        int rtPage = file->newPage();
        int infoPage = file->newPage();
        file->eof.Page = infoPage;
        file->eof.Offset = 0;
        file->writebackFileHeaderCore();
        rootPage() = rtPage;
        usedRecord() = 0;
        avalibleRecord() = 0;
        entrySession.flush();
    }
    void BTree::insertCore(Key key, std::vector<int>& trace, PageDB::Location loc) {
        Node *currentNode = new Node, *splitNode = new Node;
        int sp = trace.size() - 1;
        for(; sp > 0; sp--) {
            PageDB::PageWriteSession session1 = pgdb->GetWriteSession(file, trace[sp]);
            currentNode->ReadFromBuf(session1.buf());
            bool split = currentNode->InsertAndSplit(key, loc, *splitNode);
            currentNode->WriteToBuf(session1.buf());
            if (!split)
                break;
            int newPage = file->newPage();
            PageDB::PageWriteSession session2 = pgdb->GetWriteSession(file, newPage);
            splitNode->WriteToBuf(session2.buf());
            loc.Page = 0;
            loc.Offset = newPage;
            key = splitNode->children[0].less;
        }
        if (sp == -1) {
            currentNode->size = 2;
            currentNode->children[0].less = Key(INT_MIN, INT_MIN, INT_MIN);
            currentNode->children[0].loc = PageDB::Location(0, trace[0]);
            currentNode->children[1].less = key;
            currentNode->children[1].loc = loc;
            int newPage = file->newPage();
            PageDB::PageWriteSession session = pgdb->GetWriteSession(file, newPage);
            currentNode->WriteToBuf(session.buf());
            rootPage() = newPage;
            entrySession.flush();
        }
        delete currentNode;
        delete splitNode;
    }
    void BTree::removeCore(Key key, std::vector<int>& trace) {
        //TODO
        throw "Not Imp";
    }
}