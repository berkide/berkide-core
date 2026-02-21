#pragma once
#include <string>
#include <vector>
#include <memory>
#include <mutex>

// Split direction for window layout
// Pencere duzeni icin bolme yonu
enum class SplitDirection {
    Horizontal,  // Side by side / Yan yana
    Vertical     // Top and bottom / Ust ve alt
};

// A single editor window (leaf node in split tree)
// Tek bir editor penceresi (bolme agacinda yaprak dugum)
struct Window {
    int id = 0;                  // Unique window ID / Benzersiz pencere kimligi
    int bufferIndex = 0;         // Index into Buffers / Buffers'a dizin
    int scrollTop = 0;           // First visible line / Ilk gorunen satir
    int cursorLine = 0;          // Per-window cursor line / Pencereye ozel imlec satiri
    int cursorCol  = 0;          // Per-window cursor column / Pencereye ozel imlec sutunu
    int width  = 80;             // Window width in columns / Sutunlarda pencere genisligi
    int height = 24;             // Window height in rows / Satirlarda pencere yuksekligi
};

// A node in the split tree (either a split or a leaf window)
// Bolme agacindaki bir dugum (ya bir bolme ya da bir yaprak pencere)
struct SplitNode {
    bool isLeaf = true;                          // True if this is a window / Pencere ise true
    int windowId = -1;                           // Window ID if leaf / Yapraksa pencere kimligi
    SplitDirection direction = SplitDirection::Horizontal;  // Split direction / Bolme yonu
    double ratio = 0.5;                          // Split ratio (0.0 to 1.0) / Bolme orani
    std::unique_ptr<SplitNode> first;            // First child / Ilk cocuk
    std::unique_ptr<SplitNode> second;           // Second child / Ikinci cocuk
};

// Manages editor window layout as a binary split tree.
// Editor pencere duzenini ikili bolme agaci olarak yonetir.
// Each leaf node is a window with its own buffer reference, cursor, and scroll position.
// Her yaprak dugum kendi buffer referansi, imleci ve kayma konumuyla bir penceredir.
class WindowManager {
public:
    WindowManager();

    // Split the active window in a direction
    // Aktif pencereyi bir yonde bol
    int splitActive(SplitDirection dir);

    // Close a window by ID
    // Kimlige gore bir pencereyi kapat
    bool closeWindow(int windowId);

    // Close the active window
    // Aktif pencereyi kapat
    bool closeActive();

    // Set the active window
    // Aktif pencereyi ayarla
    bool setActive(int windowId);

    // Get the active window
    // Aktif pencereyi al
    Window* active();
    const Window* active() const;

    // Get active window ID
    // Aktif pencere kimligini al
    int activeId() const { return activeWindowId_; }

    // Get a window by ID
    // Kimlige gore bir pencere al
    Window* getWindow(int windowId);
    const Window* getWindow(int windowId) const;

    // Navigate between windows
    // Pencereler arasinda gezin
    bool focusNext();
    bool focusPrev();
    bool focusUp();
    bool focusDown();
    bool focusLeft();
    bool focusRight();

    // Resize the active split ratio
    // Aktif bolme oranini yeniden boyutlandir
    void resizeActive(double deltaRatio);

    // Equalize all splits
    // Tum bolmeleri esitle
    void equalize();

    // Get all window IDs
    // Tum pencere kimliklerini al
    std::vector<int> listWindowIds() const;

    // Get total number of windows
    // Toplam pencere sayisini al
    int windowCount() const { return static_cast<int>(windows_.size()); }

    // Get the split tree root (for client rendering)
    // Bolme agaci kokunu al (istemci renderlama icin)
    const SplitNode* root() const { return root_.get(); }

    // Set the total layout size (for calculating window dimensions)
    // Toplam duzen boyutunu ayarla (pencere boyutlarini hesaplamak icin)
    void setLayoutSize(int totalWidth, int totalHeight);

    // Recalculate window dimensions from split tree
    // Bolme agacindan pencere boyutlarini yeniden hesapla
    void recalcLayout();

private:
    mutable std::mutex mutex_;
    std::vector<Window> windows_;
    std::unique_ptr<SplitNode> root_;
    int activeWindowId_ = 0;
    int nextWindowId_ = 1;
    int totalWidth_  = 80;
    int totalHeight_ = 24;

    // Create a new window with next available ID
    // Sonraki mevcut kimlikle yeni bir pencere olustur
    Window& createWindow();

    // Find and remove a window from the split tree
    // Bolme agacindan bir pencereyi bul ve kaldir
    bool removeFromTree(SplitNode* parent, int windowId);

    // Collect all window IDs from the tree
    // Agactan tum pencere kimliklerini topla
    void collectWindowIds(const SplitNode* node, std::vector<int>& ids) const;

    // Calculate layout recursively
    // Duzeni ozyinelemeli olarak hesapla
    void calcLayout(SplitNode* node, int x, int y, int w, int h);

    // Find the split node containing a window
    // Bir pencereyi iceren bolme dugumunu bul
    SplitNode* findParentOf(SplitNode* root, int windowId);
};
