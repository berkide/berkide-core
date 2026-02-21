#include "WindowManager.h"
#include <algorithm>

// Default constructor: create initial window
// Varsayilan kurucu: ilk pencereyi olustur
WindowManager::WindowManager() {
    auto& w = createWindow();
    root_ = std::make_unique<SplitNode>();
    root_->isLeaf = true;
    root_->windowId = w.id;
    activeWindowId_ = w.id;
}

// Create a new window with next ID
// Sonraki kimlikle yeni bir pencere olustur
Window& WindowManager::createWindow() {
    Window w;
    w.id = nextWindowId_++;
    w.width = totalWidth_;
    w.height = totalHeight_;
    windows_.push_back(w);
    return windows_.back();
}

// Split the active window in a direction
// Aktif pencereyi bir yonde bol
int WindowManager::splitActive(SplitDirection dir) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Find the leaf node for active window
    // Aktif pencere icin yaprak dugumu bul
    auto findLeaf = [](SplitNode* node, int windowId, auto& self) -> SplitNode* {
        if (!node) return nullptr;
        if (node->isLeaf && node->windowId == windowId) return node;
        if (node->first) {
            auto* r = self(node->first.get(), windowId, self);
            if (r) return r;
        }
        if (node->second) {
            return self(node->second.get(), windowId, self);
        }
        return nullptr;
    };

    SplitNode* leaf = findLeaf(root_.get(), activeWindowId_, findLeaf);
    if (!leaf) return -1;

    // Create a new window
    // Yeni bir pencere olustur
    auto& newWin = createWindow();
    newWin.bufferIndex = 0;

    // Copy buffer reference from active window
    // Aktif pencereden buffer referansini kopyala
    const Window* activeWin = getWindow(activeWindowId_);
    if (activeWin) {
        newWin.bufferIndex = activeWin->bufferIndex;
    }

    // Convert leaf to split node
    // Yapragi bolme dugumune donustur
    int oldWindowId = leaf->windowId;
    leaf->isLeaf = false;
    leaf->windowId = -1;
    leaf->direction = dir;
    leaf->ratio = 0.5;

    leaf->first = std::make_unique<SplitNode>();
    leaf->first->isLeaf = true;
    leaf->first->windowId = oldWindowId;

    leaf->second = std::make_unique<SplitNode>();
    leaf->second->isLeaf = true;
    leaf->second->windowId = newWin.id;

    recalcLayout();
    return newWin.id;
}

// Close a window by ID
// Kimlige gore bir pencereyi kapat
bool WindowManager::closeWindow(int windowId) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (windows_.size() <= 1) return false;  // Can't close last window / Son pencere kapatilamaz

    // Remove from tree
    // Agactan kaldir
    if (root_->isLeaf && root_->windowId == windowId) {
        return false;  // Can't close the only window / Tek pencere kapatilamaz
    }

    bool removed = removeFromTree(root_.get(), windowId);
    if (!removed) return false;

    // Remove from windows list
    // Pencereler listesinden kaldir
    windows_.erase(
        std::remove_if(windows_.begin(), windows_.end(),
                        [windowId](const Window& w) { return w.id == windowId; }),
        windows_.end());

    // Update active window if needed
    // Gerekirse aktif pencereyi guncelle
    if (activeWindowId_ == windowId) {
        if (!windows_.empty()) {
            activeWindowId_ = windows_[0].id;
        }
    }

    recalcLayout();
    return true;
}

// Close the active window
// Aktif pencereyi kapat
bool WindowManager::closeActive() {
    return closeWindow(activeWindowId_);
}

// Remove a window from the split tree
// Bolme agacindan bir pencereyi kaldir
bool WindowManager::removeFromTree(SplitNode* node, int windowId) {
    if (!node || node->isLeaf) return false;

    // Check first child
    // Ilk cocugu kontrol et
    if (node->first && node->first->isLeaf && node->first->windowId == windowId) {
        // Replace this node with second child
        // Bu dugumu ikinci cocukla degistir
        auto second = std::move(node->second);
        *node = std::move(*second);
        return true;
    }

    // Check second child
    // Ikinci cocugu kontrol et
    if (node->second && node->second->isLeaf && node->second->windowId == windowId) {
        // Replace this node with first child
        // Bu dugumu ilk cocukla degistir
        auto first = std::move(node->first);
        *node = std::move(*first);
        return true;
    }

    // Recurse into children
    // Cocuklara ozyinelemeli git
    if (removeFromTree(node->first.get(), windowId)) return true;
    if (removeFromTree(node->second.get(), windowId)) return true;

    return false;
}

// Set the active window
// Aktif pencereyi ayarla
bool WindowManager::setActive(int windowId) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& w : windows_) {
        if (w.id == windowId) {
            activeWindowId_ = windowId;
            return true;
        }
    }
    return false;
}

// Get active window
// Aktif pencereyi al
Window* WindowManager::active() {
    return getWindow(activeWindowId_);
}

const Window* WindowManager::active() const {
    return getWindow(activeWindowId_);
}

// Get window by ID
// Kimlige gore pencere al
Window* WindowManager::getWindow(int windowId) {
    for (auto& w : windows_) {
        if (w.id == windowId) return &w;
    }
    return nullptr;
}

const Window* WindowManager::getWindow(int windowId) const {
    for (auto& w : windows_) {
        if (w.id == windowId) return &w;
    }
    return nullptr;
}

// Focus next window
// Sonraki pencereye odaklan
bool WindowManager::focusNext() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (windows_.size() <= 1) return false;

    for (size_t i = 0; i < windows_.size(); ++i) {
        if (windows_[i].id == activeWindowId_) {
            activeWindowId_ = windows_[(i + 1) % windows_.size()].id;
            return true;
        }
    }
    return false;
}

// Focus previous window
// Onceki pencereye odaklan
bool WindowManager::focusPrev() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (windows_.size() <= 1) return false;

    for (size_t i = 0; i < windows_.size(); ++i) {
        if (windows_[i].id == activeWindowId_) {
            activeWindowId_ = windows_[(i + windows_.size() - 1) % windows_.size()].id;
            return true;
        }
    }
    return false;
}

// Focus up (find window above active)
// Yukari odaklan (aktifin uzerindeki pencereyi bul)
bool WindowManager::focusUp() {
    std::lock_guard<std::mutex> lock(mutex_);
    const Window* aw = getWindow(activeWindowId_);
    if (!aw) return false;

    int bestId = -1;
    int bestDist = 999999;

    for (auto& w : windows_) {
        if (w.id == activeWindowId_) continue;
        // Window above: its cursorLine should be less
        // Yukaridaki pencere: imlec satiri daha az olmali
        if (w.cursorLine < aw->cursorLine) {
            int dist = aw->cursorLine - w.cursorLine;
            if (dist < bestDist) {
                bestDist = dist;
                bestId = w.id;
            }
        }
    }

    if (bestId >= 0) { activeWindowId_ = bestId; return true; }
    return false;
}

// Focus down
// Asagi odaklan
bool WindowManager::focusDown() {
    std::lock_guard<std::mutex> lock(mutex_);
    const Window* aw = getWindow(activeWindowId_);
    if (!aw) return false;

    int bestId = -1;
    int bestDist = 999999;

    for (auto& w : windows_) {
        if (w.id == activeWindowId_) continue;
        if (w.cursorLine > aw->cursorLine) {
            int dist = w.cursorLine - aw->cursorLine;
            if (dist < bestDist) {
                bestDist = dist;
                bestId = w.id;
            }
        }
    }

    if (bestId >= 0) { activeWindowId_ = bestId; return true; }
    return false;
}

// Focus left
// Sola odaklan
bool WindowManager::focusLeft() {
    return focusPrev();
}

// Focus right
// Saga odaklan
bool WindowManager::focusRight() {
    return focusNext();
}

// Resize active split ratio
// Aktif bolme oranini yeniden boyutlandir
void WindowManager::resizeActive(double deltaRatio) {
    std::lock_guard<std::mutex> lock(mutex_);
    SplitNode* parent = findParentOf(root_.get(), activeWindowId_);
    if (parent) {
        parent->ratio = std::max(0.1, std::min(0.9, parent->ratio + deltaRatio));
        recalcLayout();
    }
}

// Equalize all splits
// Tum bolmeleri esitle
void WindowManager::equalize() {
    std::lock_guard<std::mutex> lock(mutex_);

    auto eq = [](SplitNode* node, auto& self) -> void {
        if (!node || node->isLeaf) return;
        node->ratio = 0.5;
        self(node->first.get(), self);
        self(node->second.get(), self);
    };
    eq(root_.get(), eq);
    recalcLayout();
}

// List all window IDs
// Tum pencere kimliklerini listele
std::vector<int> WindowManager::listWindowIds() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<int> ids;
    for (auto& w : windows_) ids.push_back(w.id);
    return ids;
}

// Set total layout size
// Toplam duzen boyutunu ayarla
void WindowManager::setLayoutSize(int totalWidth, int totalHeight) {
    std::lock_guard<std::mutex> lock(mutex_);
    totalWidth_ = totalWidth;
    totalHeight_ = totalHeight;
    recalcLayout();
}

// Recalculate all window dimensions from the split tree
// Bolme agacindan tum pencere boyutlarini yeniden hesapla
void WindowManager::recalcLayout() {
    calcLayout(root_.get(), 0, 0, totalWidth_, totalHeight_);
}

// Recursive layout calculation
// Ozyinelemeli duzen hesaplama
void WindowManager::calcLayout(SplitNode* node, int x, int y, int w, int h) {
    if (!node) return;

    if (node->isLeaf) {
        Window* win = getWindow(node->windowId);
        if (win) {
            win->width = w;
            win->height = h;
        }
        return;
    }

    if (node->direction == SplitDirection::Horizontal) {
        int firstW = static_cast<int>(w * node->ratio);
        int secondW = w - firstW;
        calcLayout(node->first.get(), x, y, firstW, h);
        calcLayout(node->second.get(), x + firstW, y, secondW, h);
    } else {
        int firstH = static_cast<int>(h * node->ratio);
        int secondH = h - firstH;
        calcLayout(node->first.get(), x, y, w, firstH);
        calcLayout(node->second.get(), x, y + firstH, w, secondH);
    }
}

// Collect window IDs from tree
// Agactan pencere kimliklerini topla
void WindowManager::collectWindowIds(const SplitNode* node, std::vector<int>& ids) const {
    if (!node) return;
    if (node->isLeaf) {
        ids.push_back(node->windowId);
        return;
    }
    collectWindowIds(node->first.get(), ids);
    collectWindowIds(node->second.get(), ids);
}

// Find the parent split node of a window
// Bir pencerenin ust bolme dugumunu bul
SplitNode* WindowManager::findParentOf(SplitNode* node, int windowId) {
    if (!node || node->isLeaf) return nullptr;

    if (node->first && node->first->isLeaf && node->first->windowId == windowId) return node;
    if (node->second && node->second->isLeaf && node->second->windowId == windowId) return node;

    SplitNode* found = findParentOf(node->first.get(), windowId);
    if (found) return found;
    return findParentOf(node->second.get(), windowId);
}
