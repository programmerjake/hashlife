#ifdef __EMSCRIPTEN__
#define USE_SDL_1_x
#include <emscripten.h>
#endif // __EMSCRIPTEN__
#include <cstdlib>
#ifdef USE_SDL_1_x
#include <SDL/SDL.h>
#else
#include <SDL2/SDL.h>
#endif // USE_SDL_1_x
#include <atomic>
#include <cstdint>
#include <mutex>
#include <cassert>
#include <thread>
#include <iostream>
#include <initializer_list>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <sstream>
#include <algorithm>
#include <array>
#include <fstream>
#include "bigfloat.h"

using namespace std;

#ifdef __EMSCRIPTEN__
constexpr size_t maxNodeCount = 600000;
#else
constexpr size_t maxNodeCount = 3000000;
#endif
constexpr size_t startGCNodeCount = 6 * maxNodeCount / 7;

typedef uint_least32_t CellType;

typedef uint32_t Color;

constexpr Color RGBA(int R, int G, int B, int A)
{
    return (((Color)A & 0xFF) << 24) | (((Color)R & 0xFF) << 16) | (((Color)G & 0xFF) << 8) | ((
                Color)B & 0xFF);
}

constexpr Color RGB(int R, int G, int B)
{
    return RGBA(R, G, B, 0xFF);
}

constexpr int getR(Color c)
{
    return (int)((c >> 16) & 0xFF);
}

constexpr int getG(Color c)
{
    return (int)((c >> 8) & 0xFF);
}

constexpr int getB(Color c)
{
    return (int)(c & 0xFF);
}

constexpr int getA(Color c)
{
    return (int)((c >> 24) & 0xFF);
}

typedef Color CellColorDescriptor;

CellColorDescriptor getCellColorDescriptor(CellType cellType)
{
    switch(cellType % 8)
    {
    case 0:
        if(cellType != 0)
            return RGB(0x80, 0x80, 0x80);
        return RGB(0, 0, 0);
    case 7:
        return RGB(0, 0, 0xFF);
    case 2:
        return RGB(0, 0xFF, 0);
    case 3:
        return RGB(0, 0xFF, 0xFF);
    case 4:
        return RGB(0xFF, 0, 0);
    case 5:
        return RGB(0xFF, 0, 0xFF);
    case 6:
        return RGB(0xFF, 0xFF, 0);
    default:
        return RGB(0xFF, 0xFF, 0xFF);
    }
}

CellColorDescriptor combineCellColorDescriptors(initializer_list<CellColorDescriptor> descriptors)
{
    int R = 0, G = 0, B = 0;
    size_t count = 0;
    for(CellColorDescriptor descriptor : descriptors)
    {
        if((descriptor & RGBA(0xFF, 0xFF, 0xFF, 0)) != 0)
        {
            R += getR(descriptor);
            G += getG(descriptor);
            B += getB(descriptor);
            count++;
        }
    }

    if(count > 0)
    {
        R += count / 2;
        G += count / 2;
        B += count / 2;
        R /= count;
        G /= count;
        B /= count;
    }
    return RGB(R, G, B);
}

Color getCellColorDescriptorColor(CellColorDescriptor descriptor)
{
    return descriptor;
}

static array<array<CellType, 9>, 2> rules;

void clearRules()
{
    for(auto & row : rules)
    {
        for(auto & v : row)
        {
            v = 0;
        }
    }
}

void setLifeRules()
{
    clearRules();
    rules[0][3] = 1;
    rules[1][2] = 1;
    rules[1][3] = 1;
}

bool parseRules(string rulesString)
{
    bool gotB = false, gotSlash = false, gotS = false;
    clearRules();
    for(char ch : rulesString)
    {
        if(ch == 'B')
        {
            if(gotB)
                return false;
            gotB = true;
        }
        else if(ch == '/')
        {
            if(!gotB || gotSlash)
                return false;
            gotSlash = true;
        }
        else if(ch == 'S')
        {
            if(!gotB || !gotSlash || gotS)
                return false;
            gotS = true;
        }
        else if(ch >= '0' && ch <= '8')
        {
            if(!gotB)
                return false;
            if(gotSlash)
            {
                if(!gotS)
                    return false;
                if(rules[1][ch - '0'])
                    return false;
                rules[1][ch - '0'] = 1;
            }
            else
            {
                if(rules[0][ch - '0'])
                    return false;
                rules[0][ch - '0'] = 1;
            }
        }
        else
            return false;
    }
    return true;
}

CellType eval(CellType nxny, CellType nxcy, CellType nxpy,
              CellType cxny, CellType cxcy, CellType cxpy,
              CellType pxny, CellType pxcy, CellType pxpy)
{
    size_t count = 0;

    if(nxny != 0)
    {
        count++;
    }

    if(nxcy != 0)
    {
        count++;
    }

    if(nxpy != 0)
    {
        count++;
    }

    if(cxpy != 0)
    {
        count++;
    }

    if(pxpy != 0)
    {
        count++;
    }

    if(pxcy != 0)
    {
        count++;
    }

    if(pxny != 0)
    {
        count++;
    }

    if(cxny != 0)
    {
        count++;
    }

    if(cxcy != 0)
    {
        return rules[1][count];
    }

    return rules[0][count];
}

struct NodeGCHashTable;
struct NodeType;

class NodeReference
{
private:
    const NodeType *node;
    void incRefCount();
    void decRefCount();
    NodeReference(const NodeType *node, bool doIncrementRefcount)
        : node(node)
    {
        if(doIncrementRefcount && node != nullptr)
        {
            incRefCount();
        }
    }
public:
    NodeReference(const NodeType *node = nullptr)
        : NodeReference(node, true)
    {
    }
    NodeReference(const NodeReference &rt)
        : NodeReference(rt.node, true)
    {
    }
    NodeReference(NodeReference &&rt)
        : node(rt.node)
    {
        rt.node = nullptr;
    }
    const NodeReference &operator =(const NodeReference &rt)
    {
        if(rt.node == node)
        {
            return *this;
        }

        if(node != nullptr)
        {
            decRefCount();
        }

        node = rt.node;

        if(node != nullptr)
        {
            incRefCount();
        }

        return *this;
    }
    const NodeReference &operator =(NodeReference && rt)
    {
        const NodeType *temp = rt.node;
        rt.node = node;
        node = temp;
        return *this;
    }
    ~NodeReference()
    {
        if(node)
        {
            decRefCount();
        }
    }
    operator const NodeType *() const
    {
        return node;
    }
    operator bool() const
    {
        return node != nullptr;
    }
    bool operator !() const
    {
        return node == nullptr;
    }
    const NodeType *operator ->() const
    {
        return node;
    }
    const NodeType &operator *() const
    {
        return *node;
    }
    friend bool operator ==(const NodeReference &a, std::nullptr_t)
    {
        return a.node == nullptr;
    }
    friend bool operator ==(std::nullptr_t, const NodeReference &b)
    {
        return b.node == nullptr;
    }
    friend bool operator !=(const NodeReference &a, std::nullptr_t)
    {
        return a.node != nullptr;
    }
    friend bool operator !=(std::nullptr_t, const NodeReference &b)
    {
        return b.node != nullptr;
    }
    friend bool operator ==(const NodeReference &a, const NodeType *b)
    {
        return a.node == b;
    }
    friend bool operator ==(const NodeType *a, const NodeReference &b)
    {
        return b.node == a;
    }
    friend bool operator !=(const NodeReference &a, const NodeType *b)
    {
        return a.node != b;
    }
    friend bool operator !=(const NodeType *a, const NodeReference &b)
    {
        return b.node != a;
    }
    friend bool operator ==(const NodeReference &a, const NodeReference &b)
    {
        return a.node == b.node;
    }
    friend bool operator !=(const NodeReference &a, const NodeReference &b)
    {
        return a.node != b.node;
    }
    const NodeType *detach()
    {
        const NodeType *retval = node;
        node = nullptr;
        return retval;
    }
    static NodeReference attach(const NodeType *node)
    {
        return NodeReference(node, false);
    }
};

void dump(NodeReference rootNode);

inline void lock(std::initializer_list<atomic_bool *> locks)
{
    static size_t startDelayCount = 10000;
    size_t startCount = 0;

    for(;;)
    {
        bool anyLocked = false;

        for(auto i = locks.begin(); i != locks.end(); i++)
        {
            if(**i)
            {
                anyLocked = true;
                break;
            }
        }

        if(!anyLocked)
        {
            bool anyLocked = false;

            for(auto i = locks.begin(); i != locks.end(); i++)
            {
                if((*i)->exchange(true))
                {
                    for(auto j = locks.begin(); j != i; j++)
                    {
                        **j = false;
                    }

                    anyLocked = true;
                    break;
                }
            }

            if(!anyLocked)
            {
                return;
            }
        }

        if(startCount < startDelayCount)
        {
            startCount++;
        }
        else
        {
            std::this_thread::yield();
        }
    }
}

inline void lock(atomic_bool &theLock)
{
    lock(initializer_list<atomic_bool *> {&theLock});
}

inline void unlock(atomic_bool &theLock)
{
    theLock = false;
}

class NodeWeakReference
{
    friend struct NodeType;
private:
    mutable const NodeType *node;
    mutable const NodeWeakReference *listNext;
    mutable const NodeWeakReference *listPrev;
    mutable atomic_bool locked; // always lock starting from the front-most element
    void add();
    void remove();
public:
    NodeWeakReference(const NodeType *node = nullptr)
        : node(node), locked(false)
    {
        add();
    }
    NodeWeakReference(NodeReference node)
        : NodeWeakReference((const NodeType *)node)
    {
    }
    ~NodeWeakReference()
    {
        remove();
    }
    NodeWeakReference(const NodeWeakReference &rt)
        : locked(false)
    {
        lock(rt.locked);
        node = rt.node;
        unlock(rt.locked);
        add();
    }
    const NodeWeakReference &operator =(const NodeWeakReference &rt)
    {
        lock(initializer_list<atomic_bool *> {&rt.locked, &locked});

        if(node == rt.node)
        {
            unlock(locked);
            unlock(rt.locked);
            return *this;
        }

        const NodeType *newNode = rt.node;
        unlock(locked);
        unlock(rt.locked);
        remove();
        lock(locked);
        node = newNode;
        unlock(locked);
        add();
        return *this;
    }
    const NodeWeakReference &operator =(const NodeType *newNode)
    {
        lock(locked);

        if(node == newNode)
        {
            unlock(locked);
            return *this;
        }

        unlock(locked);
        remove();
        lock(locked);
        node = newNode;
        unlock(locked);
        add();
        return *this;
    }
    const NodeWeakReference &operator =(NodeReference newNode)
    {
        return operator =((const NodeType *)newNode);
    }
    NodeReference get() const;
};

struct NodeType
{
    NodeType(const NodeType &) = delete;
    const NodeType &operator =(const NodeType &) = delete;
    mutable atomic_uint_least8_t refcount;
    mutable uint_least8_t gcFlags = 0;
    static constexpr uint_least8_t UsedFlag = 0x1;
    bool used() const
    {
        return gcFlags & UsedFlag;
    }
    void used(bool v) const
    {
        if(v)
        {
            gcFlags |= UsedFlag;
        }
        else
        {
            gcFlags &= ~UsedFlag;
        }
    }
    mutable const NodeType *hashNext = nullptr;
    mutable const NodeType *gcNext = nullptr;  // pointer for gc uses
    mutable const NodeWeakReference *weakListHead = nullptr;
    mutable atomic_bool weakListHeadLocked;
    mutable atomic_bool removing, testingForRemove;
    mutable atomic_size_t weakGetCount;
    CellColorDescriptor overallCellColorDescriptor;
    const size_t level;
    union SectionType
    {
        CellType leaf;
        const NodeType *nonleaf;
        SectionType(CellType leaf)
            : leaf(leaf)
        {
        }
        SectionType(const NodeType *nonleaf)
            : nonleaf(nonleaf)
        {
        }
    };
    SectionType nxny;
    SectionType nxpy;
    SectionType pxny;
    SectionType pxpy;
    mutable NodeWeakReference nonleaf_nextState;
    mutable size_t nextStateLogStep;
    NodeType(CellType nxny, CellType nxpy, CellType pxny, CellType pxpy)
        : refcount(0), weakListHeadLocked(false), removing(false), testingForRemove(false), weakGetCount(0),
          level(0), nxny(nxny), nxpy(nxpy), pxny(pxny), pxpy(pxpy), nonleaf_nextState(nullptr), nextStateLogStep(0)
    {
        overallCellColorDescriptor = combineCellColorDescriptors(
                                         initializer_list<CellColorDescriptor>
        {
            getCellColorDescriptor(nxny),
            getCellColorDescriptor(nxpy),
            getCellColorDescriptor(pxny),
            getCellColorDescriptor(pxpy)
        });
    }
    NodeType(const NodeType *nxny, const NodeType *nxpy, const NodeType *pxny, const NodeType *pxpy)
        : refcount(0), weakListHeadLocked(false), removing(false), testingForRemove(false), weakGetCount(0),
          level(1 + nxny->level), nxny(nxny), nxpy(nxpy), pxny(pxny), pxpy(pxpy), nonleaf_nextState(nullptr), nextStateLogStep(nxny->level)
    {
        overallCellColorDescriptor = combineCellColorDescriptors(
                                         initializer_list<CellColorDescriptor>
        {
            nxny->overallCellColorDescriptor,
            nxpy->overallCellColorDescriptor,
            pxny->overallCellColorDescriptor,
            pxpy->overallCellColorDescriptor,
        });
    }
    ~NodeType()
    {
        removing = true;
        // nullify all weak references
        lock(weakListHeadLocked);
        const NodeWeakReference *node = weakListHead;

        while(node != nullptr)
        {
            lock(node->locked);
            const NodeWeakReference *nextNode = node->listNext;
            node->node = nullptr;
            node->listPrev = nullptr;
            node->listNext = nullptr;
            unlock(node->locked);
            node = nextNode;
        }
    }
    NodeReference getNextState(NodeGCHashTable *gc) const;
    NodeReference getNextState(NodeGCHashTable *gc, size_t logStepSize) const;
    NodeReference getCenter(NodeGCHashTable *gc) const;
};

inline void NodeReference::incRefCount()
{
    node->refcount++;
}

inline void NodeReference::decRefCount()
{
    node->refcount--;
}

inline void NodeWeakReference::add()
{
    lock(locked);
    listPrev = nullptr;
    listNext = nullptr;

    if(node != nullptr)
    {
        lock(node->weakListHeadLocked);

        if(node->weakListHead != nullptr)
        {
            lock(node->weakListHead->locked);
            node->weakListHead->listPrev = this;
            unlock(node->weakListHead->locked);
        }

        listNext = node->weakListHead;
        node->weakListHead = this;
        unlock(locked);
        unlock(node->weakListHeadLocked);
    }
    else
    {
        unlock(locked);
    }
}

inline void NodeWeakReference::remove()
{
    lock(locked);

    if(node == nullptr)
    {
        unlock(locked);
        return;
    }

    const NodeWeakReference *prev = listPrev;
    unlock(locked);

    for(;;)
    {
        // must lock in order
        atomic_bool *const pPrevLock = (prev == nullptr ? &node->weakListHeadLocked : &prev->locked);

        if(prev == nullptr)
        {
            lock(node->weakListHeadLocked);
        }
        else
        {
            lock(prev->locked);
        }

        lock(locked);

        if(prev != listPrev) // check for change while unlocked
        {
            unlock(locked);
            unlock(*pPrevLock);
            continue;
        }

        if(listPrev != nullptr)
        {
            listPrev->listNext = listNext;
        }
        else
        {
            node->weakListHead = listNext;
        }

        if(listNext != nullptr)
        {
            lock(listNext->locked);
            listNext->listPrev = listPrev;
            unlock(listNext->locked);
        }

        unlock(locked);
        unlock(*pPrevLock);
        break;
    }
}

inline NodeReference NodeWeakReference::get() const
{
    lock(locked);

    if(node == nullptr)
    {
        unlock(locked);
        return NodeReference(nullptr);
    }

    node->weakGetCount++;

    while(node->testingForRemove)
    {
        node->weakGetCount--;

        while(node->testingForRemove)
        {
            std::this_thread::yield();
        }

        node->weakGetCount++;
    }

    NodeReference retval(node);
    node->weakGetCount--;

    if(node->removing)
    {
        unlock(locked);
        return nullptr;
    }

    unlock(locked);
    return retval;
}

inline size_t hashNodeLeaf(CellType nxny, CellType nxpy, CellType pxny, CellType pxpy)
{
    size_t retval = 3;
    std::hash<CellType> hasher;
    retval += hasher(nxny) + 9 * hasher(nxpy) + (9 * 9) * hasher(pxny) + (9 * 9 * 9) * hasher(pxpy);
    return retval;
}

inline size_t hashNodeNonleaf(const NodeType *nxny, const NodeType *nxpy, const NodeType *pxny,
                const NodeType *pxpy)
{
    size_t retval = 0;
    std::hash<const NodeType *> hasher;
    retval += hasher(nxny) + 9 * hasher(nxpy) + (9 * 9) * hasher(pxny) + (9 * 9 * 9) * hasher(pxpy);
    return retval;
}

namespace std
{
template<>
struct hash<NodeType>
{
    size_t operator()(const NodeType &node) const
    {
        if(node.level == 0)
        {
            return hashNodeLeaf(node.nxny.leaf, node.nxpy.leaf, node.pxny.leaf, node.pxpy.leaf);
        }

        return hashNodeNonleaf(node.nxny.nonleaf, node.nxpy.nonleaf, node.pxny.nonleaf, node.pxpy.nonleaf);
    }
};

template <>
struct hash<NodeReference>
{
    hash<const NodeType *> hasher;
    size_t operator()(const NodeReference & node) const
    {
        return hasher((const NodeType *)node);
    }
};
}

struct NodeGCHashTable
{
    static constexpr size_t hashPrime = 1008863;
    const NodeType *table[hashPrime];
    std::mutex tableLocks[hashPrime];
    atomic_size_t nodeCount;
    atomic_bool runningGC;
    NodeGCHashTable()
        : nodeCount(0), runningGC(false)
    {
        for(const NodeType  *&node : table)
        {
            node = nullptr;
        }
    }
    NodeGCHashTable(const NodeGCHashTable &) = delete;
    const NodeGCHashTable &operator =(const NodeGCHashTable &) = delete;
    ~NodeGCHashTable()
    {
        for(size_t i = 0; i < hashPrime; i++)
        {
            const NodeType *node;
            {
                std::lock_guard<std::mutex> lock(tableLocks[i]);
                node = table[i];
                table[i] = nullptr;
            }

            while(node != nullptr)
            {
                NodeType *deleteMe = (NodeType *)node;
                node = node->hashNext;
                deleteMe->removing = true;
                delete deleteMe;
            }
        }
    }
private:
    const NodeType *clearAllNodes()
    {
        const NodeType *usedListHead = nullptr;

        for(size_t i = 0; i < hashPrime; i++)
        {
            std::lock_guard<std::mutex> lock(tableLocks[i]);
            const NodeType *node = table[i];

            while(node != nullptr)
            {
                bool used = (node->refcount > 0);
                node->used(used);

                if(used)
                {
                    node->gcNext = usedListHead;
                    usedListHead = node;
                }

                node = node->hashNext;
            }
        }

        return usedListHead;
    }
    void markNode(const NodeType *node)
    {
        if(node->used())
        {
            return;
        }

        node->used(true);

        if(node->level > 0)
        {
            markNode(node->nxny.nonleaf);
            markNode(node->nxpy.nonleaf);
            markNode(node->pxny.nonleaf);
            markNode(node->pxpy.nonleaf);
        }
    }
    void markAllNodes(const NodeType *usedListHead)
    {
        while(usedListHead != nullptr)
        {
            const NodeType *node = usedListHead;
            usedListHead = usedListHead->gcNext;
            node->gcNext = nullptr;
            node->used(true);

            if(node->level > 0)
            {
                markNode(node->nxny.nonleaf);
                markNode(node->nxpy.nonleaf);
                markNode(node->pxny.nonleaf);
                markNode(node->pxpy.nonleaf);
            }
        }
    }
    void sweepUnusedNodes()
    {
        for(size_t i = 0; i < hashPrime; i++)
        {
            std::lock_guard<std::mutex> lock(tableLocks[i]);
            const NodeType **pnode = &table[i];
            const NodeType *node = *pnode;

            while(node != nullptr)
            {
                bool used = node->used();

                if(!used)
                {
                    node->testingForRemove = true;

                    while(node->weakGetCount > 0)
                    {
                        std::this_thread::yield();
                    }

                    if(node->refcount > 0)
                    {
                        used = true;
                    }
                    else
                    {
                        node->removing = true;
                    }

                    node->testingForRemove = false;
                }

                if(used)
                {
                    pnode = &node->hashNext;
                    node = *pnode;
                }
                else
                {
                    NodeType *deleteMe = (NodeType *)node;
                    *pnode = node->hashNext;
                    nodeCount--;
                    node = *pnode;
                    delete deleteMe;
                }
            }
        }
    }
    void gc()
    {
        markAllNodes(clearAllNodes());
        sweepUnusedNodes();
    }
    void onAllocate()
    {
        if(nodeCount > startGCNodeCount)
        {
            if(runningGC.exchange(true))
            {
                while(nodeCount > maxNodeCount && runningGC)
                {
                    std::this_thread::yield();
                }
            }
            else
            {
                gc();
                runningGC = false;
            }
            if(nodeCount > maxNodeCount)
            {
                cerr << "out of memory" << endl;
                exit(1);
            }
        }
    }
public:
    NodeReference findOrInsertLeaf(CellType nxny, CellType nxpy, CellType pxny, CellType pxpy)
    {
        onAllocate();
        size_t hash = hashNodeLeaf(nxny, nxpy, pxny, pxpy) % hashPrime;
        lock_guard<std::mutex> lock(tableLocks[hash]);
        const NodeType **pnode = &table[hash];
        const NodeType *node = *pnode;

        while(node != nullptr)
        {
            if(node->level == 0 &&
                    node->nxny.leaf == nxny &&
                    node->nxpy.leaf == nxpy &&
                    node->pxny.leaf == pxny &&
                    node->pxpy.leaf == pxpy)
            {
                *pnode = node->hashNext;
                node->hashNext = table[hash];
                table[hash] = node;
                return NodeReference(node);
            }

            pnode = &node->hashNext;
            node = *pnode;
        }

        nodeCount++;
        node = new NodeType(nxny, nxpy, pxny, pxpy);
        node->hashNext = table[hash];
        table[hash] = node;
        return NodeReference(node);
    }
    NodeReference findOrInsertNonleaf(NodeReference nxny, NodeReference nxpy, NodeReference pxny,
                               NodeReference pxpy)
    {
        onAllocate();
        size_t hash = hashNodeNonleaf(nxny, nxpy, pxny, pxpy) % hashPrime;
        lock_guard<std::mutex> lock(tableLocks[hash]);
        const NodeType **pnode = &table[hash];
        const NodeType *node = *pnode;

        while(node != nullptr)
        {
            if(node->level > 0 &&
                    node->nxny.nonleaf == nxny &&
                    node->nxpy.nonleaf == nxpy &&
                    node->pxny.nonleaf == pxny &&
                    node->pxpy.nonleaf == pxpy)
            {
                *pnode = node->hashNext;
                node->hashNext = table[hash];
                table[hash] = node;
                return NodeReference(node);
            }

            pnode = &node->hashNext;
            node = *pnode;
        }

        nodeCount++;
        node = new NodeType((const NodeType *)nxny, (const NodeType *)nxpy, (const NodeType *)pxny, (const NodeType *)pxpy);
        node->hashNext = table[hash];
        table[hash] = node;
        return NodeReference(node);
    }
private:
    vector<vector<NodeReference>> nullNodes;
    atomic_bool nullNodesLocked;
public:
    NodeReference getNullNode(size_t level, CellType backgroundType)
    {
        lock(nullNodesLocked);
        if(backgroundType >= nullNodes.size())
            nullNodes.resize(backgroundType + 1);
        if(level < nullNodes[backgroundType].size())
        {
            unlock(nullNodesLocked);
            return nullNodes[backgroundType][level];
        }
        NodeReference retval;
        for(size_t i = nullNodes[backgroundType].size(); i <= level; i++)
        {
            if(i == 0)
            {
                retval = findOrInsertLeaf(backgroundType, backgroundType, backgroundType, backgroundType);
            }
            else
            {
                NodeReference prevNullNode = nullNodes[backgroundType][i - 1];
                retval = findOrInsertNonleaf(prevNullNode, prevNullNode, prevNullNode, prevNullNode);
            }
            nullNodes[backgroundType].push_back(retval);
        }
        unlock(nullNodesLocked);
        return retval;
    }
    NodeReference make4x4(CellType n2xn2y, CellType nxn2y, CellType cxn2y, CellType pxn2y,
                          CellType n2xny, CellType nxny, CellType cxny, CellType pxny,
                          CellType n2xcy, CellType nxcy, CellType cxcy, CellType pxcy,
                          CellType n2xpy, CellType nxpy, CellType cxpy, CellType pxpy)
    {
        return findOrInsertNonleaf(findOrInsertLeaf(n2xn2y, n2xny, nxn2y, nxny),
                                findOrInsertLeaf(n2xcy, n2xpy, nxcy, nxpy),
                                findOrInsertLeaf(cxn2y, cxny, pxn2y, pxny),
                                findOrInsertLeaf(cxcy, cxpy, pxcy, pxpy));
    }
};

CellType getCell(NodeReference rootNode, int x, int y);

NodeReference NodeType::getCenter(NodeGCHashTable *gc) const
{
    NodeReference thisRef = this;
    if(level == 0)
    {
        assert(false);
        return nullptr;
    }
    else if(level == 1)
        return gc->findOrInsertLeaf(nxny.nonleaf->pxpy.leaf, nxpy.nonleaf->pxny.leaf, pxny.nonleaf->nxpy.leaf, pxpy.nonleaf->nxny.leaf);
    else
        return gc->findOrInsertNonleaf(nxny.nonleaf->pxpy.nonleaf, nxpy.nonleaf->pxny.nonleaf, pxny.nonleaf->nxpy.nonleaf, pxpy.nonleaf->nxny.nonleaf);
}

NodeReference NodeType::getNextState(NodeGCHashTable *gc) const
{
    NodeReference thisRef = this;
    NodeReference retval = nonleaf_nextState.get();

    if(retval != nullptr && nextStateLogStep + 1 == level)
    {
        return retval;
    }

    if(level == 0)
    {
        assert(false);
        return nullptr;
    }
    else if(level == 1)
    {
        CellType new_nxny = eval(nxny.nonleaf->nxny.leaf, nxny.nonleaf->nxpy.leaf, nxpy.nonleaf->nxny.leaf,
                                 nxny.nonleaf->pxny.leaf, nxny.nonleaf->pxpy.leaf, nxpy.nonleaf->pxny.leaf,
                                 pxny.nonleaf->nxny.leaf, pxny.nonleaf->nxpy.leaf, pxpy.nonleaf->nxny.leaf);
        CellType new_nxpy = eval(nxny.nonleaf->nxpy.leaf, nxpy.nonleaf->nxny.leaf, nxpy.nonleaf->nxpy.leaf,
                                 nxny.nonleaf->pxpy.leaf, nxpy.nonleaf->pxny.leaf, nxpy.nonleaf->pxpy.leaf,
                                 pxny.nonleaf->nxpy.leaf, pxpy.nonleaf->nxny.leaf, pxpy.nonleaf->nxpy.leaf);
        CellType new_pxny = eval(nxny.nonleaf->pxny.leaf, nxny.nonleaf->pxpy.leaf, nxpy.nonleaf->pxny.leaf,
                                 pxny.nonleaf->nxny.leaf, pxny.nonleaf->nxpy.leaf, pxpy.nonleaf->nxny.leaf,
                                 pxny.nonleaf->pxny.leaf, pxny.nonleaf->pxpy.leaf, pxpy.nonleaf->pxny.leaf);
        CellType new_pxpy = eval(nxny.nonleaf->pxpy.leaf, nxpy.nonleaf->pxny.leaf, nxpy.nonleaf->pxpy.leaf,
                                 pxny.nonleaf->nxpy.leaf, pxpy.nonleaf->nxny.leaf, pxpy.nonleaf->nxpy.leaf,
                                 pxny.nonleaf->pxpy.leaf, pxpy.nonleaf->pxny.leaf, pxpy.nonleaf->pxpy.leaf);
        retval = gc->findOrInsertLeaf(new_nxny, new_nxpy, new_pxny, new_pxpy);
    }
    else
    {
        NodeReference step1_nxny = nxny.nonleaf->getNextState(gc);
        NodeReference step1_nxpy = nxpy.nonleaf->getNextState(gc);
        NodeReference step1_pxny = pxny.nonleaf->getNextState(gc);
        NodeReference step1_pxpy = pxpy.nonleaf->getNextState(gc);
        NodeReference step1_nxcy = gc->findOrInsertNonleaf(nxny.nonleaf->nxpy.nonleaf, nxpy.nonleaf->nxny.nonleaf, nxny.nonleaf->pxpy.nonleaf, nxpy.nonleaf->pxny.nonleaf)->getNextState(gc);
        NodeReference step1_pxcy = gc->findOrInsertNonleaf(pxny.nonleaf->nxpy.nonleaf, pxpy.nonleaf->nxny.nonleaf, pxny.nonleaf->pxpy.nonleaf, pxpy.nonleaf->pxny.nonleaf)->getNextState(gc);
        NodeReference step1_cxny = gc->findOrInsertNonleaf(nxny.nonleaf->pxny.nonleaf, nxny.nonleaf->pxpy.nonleaf, pxny.nonleaf->nxny.nonleaf, pxny.nonleaf->nxpy.nonleaf)->getNextState(gc);
        NodeReference step1_cxpy = gc->findOrInsertNonleaf(nxpy.nonleaf->pxny.nonleaf, nxpy.nonleaf->pxpy.nonleaf, pxpy.nonleaf->nxny.nonleaf, pxpy.nonleaf->nxpy.nonleaf)->getNextState(gc);
        NodeReference step1_cxcy = gc->findOrInsertNonleaf(nxny.nonleaf->pxpy.nonleaf, nxpy.nonleaf->pxny.nonleaf, pxny.nonleaf->nxpy.nonleaf, pxpy.nonleaf->nxny.nonleaf)->getNextState(gc);
        NodeReference final_nxny = gc->findOrInsertNonleaf(step1_nxny, step1_nxcy, step1_cxny, step1_cxcy)->getNextState(gc);
        NodeReference final_nxpy = gc->findOrInsertNonleaf(step1_nxcy, step1_nxpy, step1_cxcy, step1_cxpy)->getNextState(gc);
        NodeReference final_pxny = gc->findOrInsertNonleaf(step1_cxny, step1_cxcy, step1_pxny, step1_pxcy)->getNextState(gc);
        NodeReference final_pxpy = gc->findOrInsertNonleaf(step1_cxcy, step1_cxpy, step1_pxcy, step1_pxpy)->getNextState(gc);
        retval = gc->findOrInsertNonleaf(final_nxny, final_nxpy, final_pxny, final_pxpy);
    }

    nonleaf_nextState = retval;
    nextStateLogStep = level - 1;
    return retval;
}

NodeReference NodeType::getNextState(NodeGCHashTable *gc, size_t logStepSize) const
{
    NodeReference thisRef = this;
    assert(level >= logStepSize + 1);
    if(logStepSize == level - 1)
        return getNextState(gc);
    NodeReference retval = nonleaf_nextState.get();
    if(retval != nullptr && nextStateLogStep == logStepSize)
        return retval;
    NodeReference step1_nxny = nxny.nonleaf->getNextState(gc, logStepSize);
    NodeReference step1_nxpy = nxpy.nonleaf->getNextState(gc, logStepSize);
    NodeReference step1_pxny = pxny.nonleaf->getNextState(gc, logStepSize);
    NodeReference step1_pxpy = pxpy.nonleaf->getNextState(gc, logStepSize);
    NodeReference step1_nxcy = gc->findOrInsertNonleaf(nxny.nonleaf->nxpy.nonleaf, nxpy.nonleaf->nxny.nonleaf, nxny.nonleaf->pxpy.nonleaf, nxpy.nonleaf->pxny.nonleaf)->getNextState(gc, logStepSize);
    NodeReference step1_pxcy = gc->findOrInsertNonleaf(pxny.nonleaf->nxpy.nonleaf, pxpy.nonleaf->nxny.nonleaf, pxny.nonleaf->pxpy.nonleaf, pxpy.nonleaf->pxny.nonleaf)->getNextState(gc, logStepSize);
    NodeReference step1_cxny = gc->findOrInsertNonleaf(nxny.nonleaf->pxny.nonleaf, nxny.nonleaf->pxpy.nonleaf, pxny.nonleaf->nxny.nonleaf, pxny.nonleaf->nxpy.nonleaf)->getNextState(gc, logStepSize);
    NodeReference step1_cxpy = gc->findOrInsertNonleaf(nxpy.nonleaf->pxny.nonleaf, nxpy.nonleaf->pxpy.nonleaf, pxpy.nonleaf->nxny.nonleaf, pxpy.nonleaf->nxpy.nonleaf)->getNextState(gc, logStepSize);
    NodeReference step1_cxcy = gc->findOrInsertNonleaf(nxny.nonleaf->pxpy.nonleaf, nxpy.nonleaf->pxny.nonleaf, pxny.nonleaf->nxpy.nonleaf, pxpy.nonleaf->nxny.nonleaf)->getNextState(gc, logStepSize);
    NodeReference final_nxny = gc->findOrInsertNonleaf(step1_nxny, step1_nxcy, step1_cxny, step1_cxcy)->getCenter(gc);
    NodeReference final_nxpy = gc->findOrInsertNonleaf(step1_nxcy, step1_nxpy, step1_cxcy, step1_cxpy)->getCenter(gc);
    NodeReference final_pxny = gc->findOrInsertNonleaf(step1_cxny, step1_cxcy, step1_pxny, step1_pxcy)->getCenter(gc);
    NodeReference final_pxpy = gc->findOrInsertNonleaf(step1_cxcy, step1_cxpy, step1_pxcy, step1_pxpy)->getCenter(gc);
    retval = gc->findOrInsertNonleaf(final_nxny, final_nxpy, final_pxny, final_pxpy);
    nonleaf_nextState = retval;
    nextStateLogStep = logStepSize;
    return retval;
}

inline void drawPixel(int x, int y, Color color, void *pixels, int w, int h, int pitch)
{
    if(x >= 0 && y >= 0 && x < w && y < h)
    {
        *(Color *)((char *)(pixels) + (x * sizeof(Color) + y * pitch)) = color;
    }
}

inline void drawPixel(BigFloat x, BigFloat y, Color color, void *pixels, int w, int h, int pitch)
{
    if(x >= 0 && y >= 0 && x < w && y < h)
    {
        drawPixel((int)x, (int)y, color, pixels, w, h, pitch);
    }
}

inline void drawSquare(int x, int y, int size, Color color, void *pixels, int w, int h, int pitch)
{
    for(int ry = max(0, -y); ry < size && ry + y < h; ry++)
    {
        for(int rx = max(0, -x); rx < size && rx + x < w; rx++)
        {
            drawPixel(x + rx, y + ry, color, pixels, w, h, pitch);
        }
    }
}

inline void drawSquare(BigFloat x, BigFloat y, BigFloat size, Color color, void *pixels, int w, int h, int pitch)
{
    if(x + size < 0 || x >= w)
        return;
    if(y + size < 0 || y >= h)
        return;
    BigFloat xSize = size;
    BigFloat ySize = size;
    if(x < 0)
    {
        xSize += x;
        x = 0;
    }
    if(y < 0)
    {
        ySize += y;
        y = 0;
    }
    if(x + xSize > w - 1)
    {
        xSize = w - 1 - x;
    }
    if(y + ySize > h - 1)
    {
        ySize = h - 1 - y;
    }
    drawSquare((int)x, (int)y, (int)max(xSize, ySize), color, pixels, w, h, pitch);
}

void drawNode(NodeReference node, BigFloat centerX, BigFloat centerY, int logSize, void *pixels, int w, int h, int pitch)
{
    if(logSize <= 0)
    {
        drawPixel(centerX, centerY, getCellColorDescriptorColor(node->overallCellColorDescriptor), pixels, w, h, pitch);
        return;
    }
    if(node->level == 0)
    {
        assert(logSize > 0);
        BigFloat pixelSize = ldexp(1_bf, logSize - 1);
        drawSquare(centerX - pixelSize, centerY - pixelSize, pixelSize, getCellColorDescriptorColor(getCellColorDescriptor(node->nxny.leaf)), pixels, w, h, pitch);
        drawSquare(centerX - pixelSize, centerY, pixelSize, getCellColorDescriptorColor(getCellColorDescriptor(node->nxpy.leaf)), pixels, w, h, pitch);
        drawSquare(centerX, centerY - pixelSize, pixelSize, getCellColorDescriptorColor(getCellColorDescriptor(node->pxny.leaf)), pixels, w, h, pitch);
        drawSquare(centerX, centerY, pixelSize, getCellColorDescriptorColor(getCellColorDescriptor(node->pxpy.leaf)), pixels, w, h, pitch);
        return;
    }
    BigFloat subNodeSize = ldexp(1_bf, logSize - 1);
    BigFloat halfSubNodeSize = subNodeSize / 2;
    if(centerX + subNodeSize <= 0 || centerY + subNodeSize <= 0 || centerX - subNodeSize > w || centerY - subNodeSize > h)
        return;
    drawNode(node->nxny.nonleaf, centerX - halfSubNodeSize, centerY - halfSubNodeSize, logSize - 1, pixels, w, h, pitch);
    drawNode(node->nxpy.nonleaf, centerX - halfSubNodeSize, centerY + halfSubNodeSize, logSize - 1, pixels, w, h, pitch);
    drawNode(node->pxny.nonleaf, centerX + halfSubNodeSize, centerY - halfSubNodeSize, logSize - 1, pixels, w, h, pitch);
    drawNode(node->pxpy.nonleaf, centerX + halfSubNodeSize, centerY + halfSubNodeSize, logSize - 1, pixels, w, h, pitch);
}

NodeReference setCellH(NodeReference node, BigFloat centerX, BigFloat centerY, NodeGCHashTable * gc, int x, int y, CellType newCell)
{
    BigFloat subNodeSize = ldexp(1_bf, node->level);
    if(x < centerX - subNodeSize || x >= centerX + subNodeSize || y < centerY - subNodeSize || y >= centerY + subNodeSize)
        assert(false);
    if(node->level == 0)
    {
        CellType nxny = node->nxny.leaf;
        CellType nxpy = node->nxpy.leaf;
        CellType pxny = node->pxny.leaf;
        CellType pxpy = node->pxpy.leaf;
        if(x == centerX - 1 && y == centerY - 1)
            nxny = newCell;
        else if(x == centerX - 1 && y == centerY)
            nxpy = newCell;
        else if(x == centerX && y == centerY - 1)
            pxny = newCell;
        else if(x == centerX && y == centerY)
            pxpy = newCell;
        else
            assert(false);
        return gc->findOrInsertLeaf(nxny, nxpy, pxny, pxpy);
    }
    BigFloat halfSubNodeSize = subNodeSize / 2;
    NodeReference nxny = node->nxny.nonleaf;
    NodeReference nxpy = node->nxpy.nonleaf;
    NodeReference pxny = node->pxny.nonleaf;
    NodeReference pxpy = node->pxpy.nonleaf;
    if(x < centerX)
    {
        if(y < centerY)
        {
            nxny = setCellH(nxny, centerX - halfSubNodeSize, centerY - halfSubNodeSize, gc, x, y, newCell);
            assert(nxny != nullptr);
        }
        else
        {
            nxpy = setCellH(nxpy, centerX - halfSubNodeSize, centerY + halfSubNodeSize, gc, x, y, newCell);
            assert(nxpy != nullptr);
        }
    }
    else
    {
        if(y < centerY)
        {
            pxny = setCellH(pxny, centerX + halfSubNodeSize, centerY - halfSubNodeSize, gc, x, y, newCell);
            assert(pxny != nullptr);
        }
        else
        {
            pxpy = setCellH(pxpy, centerX + halfSubNodeSize, centerY + halfSubNodeSize, gc, x, y, newCell);
            assert(pxpy != nullptr);
        }
    }
    return gc->findOrInsertNonleaf(nxny, nxpy, pxny, pxpy);
}

bool isInNodeBounds(NodeReference node, BigFloat centerX, BigFloat centerY, int x, int y)
{
    BigFloat subNodeSize = ldexp(1_bf, node->level);
    if(x < centerX - subNodeSize || x >= centerX + subNodeSize || y < centerY - subNodeSize || y >= centerY + subNodeSize)
        return false;
    return true;
}

CellType getCellH(NodeReference node, BigFloat centerX, BigFloat centerY, int x, int y)
{
    BigFloat subNodeSize = ldexp(1_bf, node->level);
    if(x < centerX - subNodeSize || x >= centerX + subNodeSize || y < centerY - subNodeSize || y >= centerY + subNodeSize)
        assert(false);
    if(node->level == 0)
    {
        CellType nxny = node->nxny.leaf;
        CellType nxpy = node->nxpy.leaf;
        CellType pxny = node->pxny.leaf;
        CellType pxpy = node->pxpy.leaf;
        if(x == centerX - 1 && y == centerY - 1)
            return nxny;
        else if(x == centerX - 1 && y == centerY)
            return nxpy;
        else if(x == centerX && y == centerY - 1)
            return pxny;
        else if(x == centerX && y == centerY)
            return pxpy;
        else
        {
            assert(false);
            return 0;
        }
    }
    BigFloat halfSubNodeSize = subNodeSize / 2;
    if(x < centerX)
    {
        if(y < centerY)
        {
            NodeReference nxny = node->nxny.nonleaf;
            return getCellH(nxny, centerX - halfSubNodeSize, centerY - halfSubNodeSize, x, y);
        }
        else
        {
            NodeReference nxpy = node->nxpy.nonleaf;
            return getCellH(nxpy, centerX - halfSubNodeSize, centerY + halfSubNodeSize, x, y);
        }
    }
    else
    {
        if(y < centerY)
        {
            NodeReference pxny = node->pxny.nonleaf;
            return getCellH(pxny, centerX + halfSubNodeSize, centerY - halfSubNodeSize, x, y);
        }
        else
        {
            NodeReference pxpy = node->pxpy.nonleaf;
            return getCellH(pxpy, centerX + halfSubNodeSize, centerY + halfSubNodeSize, x, y);
        }
    }
}

struct GameState
{
    NodeGCHashTable * gc;
    NodeReference rootNode;
    CellType backgroundType;
    GameState(NodeGCHashTable * gc, NodeReference rootNode = nullptr, CellType backgroundType = 0)
        : gc(gc), rootNode(rootNode), backgroundType(backgroundType)
    {
        if(gc == nullptr)
        {
            this->rootNode = nullptr;
            this->backgroundType = 0;
            return;
        }
        if(rootNode == nullptr)
            this->rootNode = gc->getNullNode(0, backgroundType);
        assert(this->rootNode != nullptr);
    }
    void draw(int logSize, void *pixels, int w, int h, int pitch)
    {
        assert(gc != nullptr);
        drawSquare(0, 0, max(w, h), getCellColorDescriptorColor(getCellColorDescriptor(backgroundType)), pixels, w, h, pitch);
        drawNode(rootNode, w / 2, h / 2, logSize + 1, pixels, w, h, pitch);
    }
private:
    void expandRoot()
    {
        assert(gc != nullptr);
        if(rootNode->level == 0)
        {
            rootNode = gc->findOrInsertNonleaf(gc->findOrInsertLeaf(backgroundType, backgroundType, backgroundType, rootNode->nxny.leaf),
                                    gc->findOrInsertLeaf(backgroundType, backgroundType, rootNode->nxpy.leaf, backgroundType),
                                    gc->findOrInsertLeaf(backgroundType, rootNode->pxny.leaf, backgroundType, backgroundType),
                                    gc->findOrInsertLeaf(rootNode->pxpy.leaf, backgroundType, backgroundType, backgroundType));
        }
        else
        {
            NodeReference nullNode = gc->getNullNode(rootNode->level - 1, backgroundType);
            rootNode = gc->findOrInsertNonleaf(gc->findOrInsertNonleaf(nullNode, nullNode, nullNode, rootNode->nxny.nonleaf),
                                    gc->findOrInsertNonleaf(nullNode, nullNode, rootNode->nxpy.nonleaf, nullNode),
                                    gc->findOrInsertNonleaf(nullNode, rootNode->pxny.nonleaf, nullNode, nullNode),
                                    gc->findOrInsertNonleaf(rootNode->pxpy.nonleaf, nullNode, nullNode, nullNode));
        }
    }
public:
    void setCell(int x, int y, CellType newCell)
    {
        assert(gc != nullptr);
        while(!isInNodeBounds(rootNode, 0, 0, x, y))
        {
            expandRoot();
        }
        rootNode = setCellH(rootNode, 0, 0, gc, x, y, newCell);
    }
    CellType getCell(int x, int y)
    {
        assert(gc != nullptr);
        if(!isInNodeBounds(rootNode, 0, 0, x, y))
            return backgroundType;
        return getCellH(rootNode, 0, 0, x, y);
    }
private:
    void checkForContractRoot()
    {
        assert(gc != nullptr);
        for(;;)
        {
            if(rootNode->level < 2)
            {
                return;
            }
            NodeReference nullNode = gc->getNullNode(rootNode->level - 2, backgroundType);
            if(rootNode->nxny.nonleaf->nxny.nonleaf != nullNode)
                return;
            if(rootNode->nxny.nonleaf->nxpy.nonleaf != nullNode)
                return;
            if(rootNode->nxny.nonleaf->pxny.nonleaf != nullNode)
                return;
            if(rootNode->nxpy.nonleaf->nxny.nonleaf != nullNode)
                return;
            if(rootNode->nxpy.nonleaf->nxpy.nonleaf != nullNode)
                return;
            if(rootNode->nxpy.nonleaf->pxpy.nonleaf != nullNode)
                return;
            if(rootNode->pxny.nonleaf->nxny.nonleaf != nullNode)
                return;
            if(rootNode->pxny.nonleaf->pxny.nonleaf != nullNode)
                return;
            if(rootNode->pxny.nonleaf->pxpy.nonleaf != nullNode)
                return;
            if(rootNode->pxpy.nonleaf->nxpy.nonleaf != nullNode)
                return;
            if(rootNode->pxpy.nonleaf->pxny.nonleaf != nullNode)
                return;
            if(rootNode->pxpy.nonleaf->pxpy.nonleaf != nullNode)
                return;
            rootNode = rootNode->getCenter(gc);
        }
    }
public:
    void step(size_t logStepSize)
    {
        assert(gc != nullptr);
        expandRoot();
        expandRoot();
        while(rootNode->level < logStepSize + 1)
            expandRoot();
        backgroundType = getCellH(gc->getNullNode(rootNode->level, backgroundType)->getNextState(gc, logStepSize), 0, 0, 0, 0);
        rootNode = rootNode->getNextState(gc, logStepSize);
        checkForContractRoot();
    }
    operator bool() const
    {
        return gc != nullptr;
    }
    bool operator !() const
    {
        return gc == nullptr;
    }
};

string getCellStringNoPrefix(CellType cellType)
{
    if(cellType)
        return "#";
    return "-";
}

string getCellString(CellType cellType)
{
    return "C" + getCellStringNoPrefix(cellType);
}

string getNodeString(int nodeIndex)
{
    ostringstream os;
    os << "N" << nodeIndex;
    return os.str();
}

vector<vector<CellType>> getNodeGraph(NodeReference node)
{
    if(node->level == 0)
    {
        return vector<vector<CellType>>{vector<CellType>{node->nxny.leaf, node->pxny.leaf}, vector<CellType>{node->nxpy.leaf, node->pxpy.leaf}};
    }
    vector<vector<CellType>> nxny = getNodeGraph(node->nxny.nonleaf);
    vector<vector<CellType>> nxpy = getNodeGraph(node->nxpy.nonleaf);
    vector<vector<CellType>> pxny = getNodeGraph(node->pxny.nonleaf);
    vector<vector<CellType>> pxpy = getNodeGraph(node->pxpy.nonleaf);
    vector<vector<CellType>> retval;
    size_t subNodeSize = (size_t)1 << node->level;
    retval.reserve(2 * subNodeSize);
    for(size_t y = 0; y < subNodeSize; y++)
    {
        retval.push_back(std::move(nxny[y]));
        retval.back().reserve(2 * subNodeSize);
        retval.back().insert(retval.back().end(), pxny[y].begin(), pxny[y].end());
    }
    for(size_t y = 0; y < subNodeSize; y++)
    {
        retval.push_back(std::move(nxpy[y]));
        retval.back().reserve(2 * subNodeSize);
        retval.back().insert(retval.back().end(), pxpy[y].begin(), pxpy[y].end());
    }
    return std::move(retval);
}

string getNodeGraphAsString(NodeReference node)
{
    vector<vector<CellType>> theGraph = getNodeGraph(node);
    ostringstream os;
    for(const vector<CellType> & line : theGraph)
    {
        string seperator = "";
        for(CellType cell : line)
        {
            string cellString = getCellStringNoPrefix(cell);
            cellString.resize(2, ' ');
            os << seperator << cellString;
            seperator = " ";
        }
        os << "\n";
    }
    return os.str();
}

void dump(NodeReference rootNode)
{
    unordered_map<NodeReference, int> nodesMap;
    int nextNodeIndex = 1;
    vector<NodeReference> nodesList;
    unordered_set<NodeReference> newNodesSet;
    newNodesSet.insert(rootNode);
    while(!newNodesSet.empty())
    {
        unordered_set<NodeReference> nodesSet(std::move(newNodesSet));
        newNodesSet.clear();
        for(NodeReference node : nodesSet)
        {
            if(nodesMap.count(node) == 0)
            {
                nodesMap[node] = nextNodeIndex++;
                nodesList.push_back(node);
                if(node->level > 0)
                {
                    newNodesSet.insert(node->nxny.nonleaf);
                    newNodesSet.insert(node->nxpy.nonleaf);
                    newNodesSet.insert(node->pxny.nonleaf);
                    newNodesSet.insert(node->pxpy.nonleaf);
                }
            }
        }
    }
    std::reverse(nodesList.begin(), nodesList.end());
    for(NodeReference node : nodesList)
    {
        string nodeName = getNodeString(nodesMap[node]);
        nodeName.resize(10, ' ');
        cout << nodeName << " : " << node->level << "\n    ";
        string nxny, nxpy, pxny, pxpy;
        if(node->level == 0)
        {
            nxny = getCellString(node->nxny.leaf);
            nxpy = getCellString(node->nxpy.leaf);
            pxny = getCellString(node->pxny.leaf);
            pxpy = getCellString(node->pxpy.leaf);
        }
        else
        {
            nxny = getNodeString(nodesMap[node->nxny.nonleaf]);
            nxpy = getNodeString(nodesMap[node->nxpy.nonleaf]);
            pxny = getNodeString(nodesMap[node->pxny.nonleaf]);
            pxpy = getNodeString(nodesMap[node->pxpy.nonleaf]);
        }
        nxny.resize(10, ' ');
        nxpy.resize(10, ' ');
        pxny.resize(10, ' ');
        pxpy.resize(10, ' ');
        cout << nxny << " " << pxny << "\n    " << nxpy << " " << pxpy << "\n";
        if(node->level <= 3)
        {
            cout << "\n" << getNodeGraphAsString(node);
        }
        cout << "\n";
    }
    cout << flush;
}

GameState readRLE(istream & is, NodeGCHashTable * gc)
{
    cout << "reading ...\x1b[K\r" << flush;
    GameState retval = GameState(gc);
    char xch, eq1, comma, ych, eq2, comma2, eq3;
    int w, h;
    string rule, ruleName;
    while(is.peek() == '#')
    {
        is.ignore(10000, '\n');
    }
    is >> xch >> eq1 >> w >> comma >> ych >> eq2 >> h >> comma2 >> ruleName >> eq3 >> rule;
    is.ignore(10000, '\n');
    if(!is)
    {
        cout << "read failed.\x1b[K\n" << flush;
        return nullptr;
    }
    if(!parseRules(rule))
    {
        setLifeRules();
        cout << "read failed.\x1b[K\n" << flush;
        return nullptr;
    }
    int x = 0, y = 0;
    size_t currentCount = 0, popCount = 0;
    while(is)
    {
        int ch = is.get();
        if(ch >= '0' && ch <= '9')
        {
            currentCount *= 10;
            currentCount += ch - '0';
        }
        else if(ch == 'b' || ch == '.')
        {
            if(currentCount == 0)
                currentCount = 1;
            x += currentCount;
            currentCount = 0;
        }
        else if(ch == 'o')
        {
            if(currentCount == 0)
                currentCount = 1;
            for(size_t i = 0; i < currentCount; i++, x++)
            {
                retval.setCell(x, y, 1);
                if(++popCount % 1000 == 0)
                    cout << "reading ... " << popCount << "\x1b[K\r" << flush;
            }
            currentCount = 0;
        }
        else if(ch >= 'A' && ch <= 'X')
        {
            if(currentCount == 0)
                currentCount = 1;
            for(size_t i = 0; i < currentCount; i++, x++)
            {
                retval.setCell(x, y, 1 + (int)ch - 'A');
                if(++popCount % 1000 == 0)
                    cout << "reading ... " << popCount << "\x1b[K\r" << flush;
            }
            currentCount = 0;
        }
        else if(ch == 'p')
        {
            if(currentCount == 0)
                currentCount = 1;
            ch = is.get();
            if(ch >= 'A' && ch <= 'X')
            {
                for(size_t i = 0; i < currentCount; i++, x++)
                {
                    retval.setCell(x, y, 25 + (int)ch - 'A');
                    if(++popCount % 1000 == 0)
                        cout << "reading ... " << popCount << "\x1b[K\r" << flush;
                }
            }
            else
            {
                cout << "read failed.\x1b[K\n" << flush;
                return nullptr;
            }
            currentCount = 0;
        }
        else if(ch >= 'q' && ch < 'y')
        {
            if(currentCount == 0)
                currentCount = 1;
            char oldCh = ch;
            ch = is.get();
            if(ch >= 'A' && ch <= 'Z')
            {
                for(size_t i = 0; i < currentCount; i++, x++)
                {
                    retval.setCell(x, y, 49 + 26 * (int)(oldCh - 'q') + (int)ch - 'A');
                    if(++popCount % 1000 == 0)
                        cout << "reading ... " << popCount << "\x1b[K\r" << flush;
                }
            }
            else
            {
                cout << "read failed.\x1b[K\n" << flush;
                return nullptr;
            }
            currentCount = 0;
        }
        else if(ch == 'y')
        {
            if(currentCount == 0)
                currentCount = 1;
            ch = is.get();
            if(ch >= 'A' && ch <= 'O')
            {
                for(size_t i = 0; i < currentCount; i++, x++)
                {
                    retval.setCell(x, y, 241 + (int)ch - 'A');
                    if(++popCount % 1000 == 0)
                        cout << "reading ... " << popCount << "\x1b[K\r" << flush;
                }
            }
            else
            {
                cout << "read failed.\x1b[K\n" << flush;
                return nullptr;
            }
            currentCount = 0;
        }
        else if(ch == '$')
        {
            if(currentCount == 0)
                currentCount = 1;
            x = 0;
            y += currentCount;
            currentCount = 0;
        }
        else if(ch == '!')
        {
            cout << "read.\x1b[K\n" << flush;
            return retval;
        }
        else if(ch == ' ' || ch == '\r' || ch == '\n' || ch == '\t')
        {
        }
        else
        {
            cout << "read failed.\x1b[K\n" << flush;
            return nullptr;
        }
    }
    cout << "read failed.\x1b[K\n" << flush;
    return nullptr;
}

int main(int argc, char ** argv)
{
    setLifeRules();
    string fName = "pattern.rle";
    bool gotPattern = false;
    for(int i = 1; i < argc; i++)
    {
        string arg = argv[i];
        if(arg == "-h" || arg == "--help" || gotPattern)
        {
            cout << "usage : hashlife [-h|--help] [<pattern file name>]\n";
            return 0;
        }
        else
            fName = arg;
    }
    ifstream rleStream(fName.c_str());
    cout << "reading '" << fName << "'...\n";
    static auto gc = new NodeGCHashTable;
    static GameState gs = readRLE(rleStream, gc);
    rleStream.close();
    if(!gs)
        return 1;
    //dump(rootNode);
    //cout << endl << endl;
    //dump(stepRoot(rootNode, gc, 5));
    //return 0;
    // initialize SDL video
    if(SDL_Init(SDL_INIT_VIDEO) < 0)
    {
        printf("Unable to init SDL: %s\n", SDL_GetError());
        return 1;
    }

    // make sure SDL cleans up before exit
    atexit(SDL_Quit);
#ifdef USE_SDL_1_x
#ifndef __EMSCRIPTEN__
    const int w = 1024, h = 768;
#else
    static int w, h;
    w = 1 << 20;
    {
        int aw = emscripten_run_script_int("window.innerWidth");
        const int extraAreaForText = 100;
        int ah = emscripten_run_script_int("window.innerHeight") - extraAreaForText;
        emscripten_log(0, "%dx%d", aw, ah);
        while((aw < w || ah < w) && w > 1)
            w /= 2;
    }
    h = w;
#endif // __EMSCRIPTEN__
    static SDL_Surface *screen;
    screen = SDL_SetVideoMode(w, h, 32, SDL_ANYFORMAT);
    if(!screen)
    {
        printf("Unable to create screen: %s\n", SDL_GetError());
        return 1;
    }
    static SDL_Surface *texture;
    texture = SDL_CreateRGBSurface(SDL_SWSURFACE, w, h, 32, RGBA(0xFF, 0, 0, 0), RGBA(0, 0xFF, 0, 0), RGBA(0, 0, 0xFF, 0), RGBA(0, 0, 0, 0xFF));
    if(!texture)
    {
        printf("Unable to create texture: %s\n", SDL_GetError());
        return 1;
    }
#else
    SDL_Window *window = SDL_CreateWindow("HashLife", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                                          1024, 768, (SDL_WINDOW_FULLSCREEN_DESKTOP, 0));

    if(!window)
    {
        printf("Unable to create window: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, 0);

    if(!renderer)
    {
        printf("Unable to create renderer: %s\n", SDL_GetError());
        return 1;
    }

    int w, h;
    SDL_GetWindowSize(window, &w, &h);
    SDL_Texture *texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                           SDL_TEXTUREACCESS_STREAMING, w, h);

    if(!texture)
    {
        printf("Unable to create texture: %s\n", SDL_GetError());
        return 1;
    }
#endif

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop([](){
#endif
    static bool done = false, canPause = false;
    static size_t stepSize = 0;

#ifndef __EMSCRIPTEN__
    while(!done)
    {
#endif
        // message processing loop
        SDL_Event event;

        bool doStep = false;

#ifdef __EMSCRIPTEN__
        while(SDL_PollEvent(&event))
#else
        while(canPause ? SDL_WaitEvent(&event) : SDL_PollEvent(&event))
#endif
        {
            // check for messages
            switch(event.type)
            {
            // exit if the window is closed
            case SDL_QUIT:
                done = true;
                canPause = false;
                break;

            // check for keypresses
            case SDL_KEYDOWN:
            {
                // exit if ESCAPE is pressed
                if(event.key.keysym.sym == SDLK_ESCAPE)
                {
                    done = true;
                    canPause = false;
                }
                if(event.key.keysym.sym == SDLK_SPACE)
                {
                    doStep = true;
                    canPause = false;
                }
                if(event.key.keysym.sym == SDLK_PLUS || event.key.keysym.sym == SDLK_EQUALS || event.key.keysym.sym == SDLK_a)
                {
                    stepSize++;
                    canPause = false;
                }
                if(event.key.keysym.sym == SDLK_UNDERSCORE || event.key.keysym.sym == SDLK_MINUS || event.key.keysym.sym == SDLK_z)
                {
                    if(stepSize > 0)
                        stepSize--;
                    canPause = false;
                }

                break;
            }
            } // end switch
        } // end of message processing
#ifdef __EMSCRIPTEN__
        ostringstream textStream;
        textStream
#else
        cout
#endif
         << "Step Size : " << stepSize << "     Level : " << gs.rootNode->level;
#ifdef __EMSCRIPTEN__
        static string lastLogString = "";
        string logString = textStream.str();
        if(lastLogString != logString)
        {
            emscripten_log(0, "%s", logString.c_str());
            lastLogString = logString;
        }
#else
        cout << "\x1b[K\r" << flush;
#endif // __EMSCRIPTEN__
        if(doStep)
            gs.step(stepSize);


        void *pixels;
        int pitch;
#ifdef USE_SDL_1_x
        SDL_LockSurface(texture);
        pixels = texture->pixels;
        pitch = texture->pitch;
#else
        SDL_LockTexture(texture, nullptr, &pixels, &pitch);
#endif // USE_SDL_1_x
        gs.draw(8, pixels, w, h, pitch);
#ifdef USE_SDL_1_x
        SDL_UnlockSurface(texture);
        SDL_BlitSurface(texture, nullptr, screen, nullptr);
        SDL_Flip(screen);
#else
        SDL_UnlockTexture(texture);
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, nullptr, nullptr);
        SDL_RenderPresent(renderer);
#endif
#ifndef __EMSCRIPTEN__
        if(!doStep)
            canPause = true;
#endif
    } // end main loop
#ifdef __EMSCRIPTEN__
    , 0, true);
#endif
    return 0;
}



