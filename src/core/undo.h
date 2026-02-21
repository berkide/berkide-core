#pragma once
#include <memory>
#include <string>
#include <vector>
#include "buffer.h"

// Types of actions that can be undone/redone
// Geri alinabilir/yinelenebilir eylem turleri
enum class ActionType {
    Insert,       // Single char insert / Tek karakter ekleme
    Delete,       // Single char delete / Tek karakter silme
    InsertLine,   // Insert a whole line / Bir satir ekleme
    DeleteLine,   // Delete a whole line / Bir satir silme
    InsertText,   // Multi-char text insert (may contain newlines) / Cok karakterli metin ekleme (yeni satir icerebilir)
    DeleteRange   // Delete text between two positions / Iki konum arasindaki metni silme
};

// A single undoable action with its position and data
// Konumu ve verisiyle birlikte tek bir geri alinabilir eylem
struct Action {
    ActionType type;
    int line;
    int col;
    char character = '\0';        // For single char insert/delete / Tek karakter ekleme/silme icin
    std::string lineContent;      // For line insert/delete or multi-char text / Satir veya cok karakterli metin icin
    int lineEnd = -1;             // End line for DeleteRange / DeleteRange icin bitis satiri
    int colEnd = -1;              // End column for DeleteRange / DeleteRange icin bitis sutunu
};

// A node in the undo tree (supports branching undo history)
// Geri alma agacindaki bir dugum (dallanan geri alma gecmisini destekler)
struct UndoNode {
    Action action;
    std::shared_ptr<UndoNode> parent;                    // Parent node / Ana dugum
    std::vector<std::shared_ptr<UndoNode>> branches;     // Child branches / Alt dallar
    int activeBranch = -1;                                // Currently active branch / Su an aktif dal
    int groupSize = 0;                                    // Number of actions in this group (0 = not a group end) / Bu gruptaki eylem sayisi (0 = grup sonu degil)
};

// Manages undo/redo operations with tree-based branching history.
// Agac tabanli dallanan gecmisle geri alma/yineleme islemlerini yonetir.
// Unlike linear undo, this preserves all edit branches.
// Dogrusal geri almanin aksine, tum duzenleme dallarini korur.
class UndoManager {
public:
    UndoManager();

    // Record a new action (creates a new node in the undo tree)
    // Yeni bir eylem kaydet (geri alma agacinda yeni bir dugum olusturur)
    void addAction(const Action& action);

    // Begin a group of actions that undo/redo as a single step
    // Tek adim olarak geri alinacak/yinelenecek bir eylem grubu baslat
    void beginGroup();

    // End the current action group
    // Mevcut eylem grubunu bitir
    void endGroup();

    // Check if currently inside a group
    // Su an bir grup icinde olup olmadigini kontrol et
    bool inGroup() const;

    // Undo the last action (or group), modifying the buffer accordingly
    // Son eylemi (veya grubu) geri al, buffer'i buna gore degistir
    bool undo(Buffer& buf);

    // Redo the next action (or group) in the current branch
    // Mevcut daldaki sonraki eylemi (veya grubu) yinele
    bool redo(Buffer& buf);

    // Switch to a different branch at the current node
    // Mevcut dugumde farkli bir dala gec
    void branch(int index);

    // Get the number of branches at the current node
    // Mevcut dugumdeki dal sayisini al
    int branchCount() const;

    // Get the active branch index at the current node
    // Mevcut dugumdeki aktif dal indeksini al
    int currentBranch() const;

private:
    // Apply an undo operation to the buffer
    // Buffer'a bir geri alma islemi uygula
    void applyUndo(const Action& action, Buffer& buf);

    // Apply a redo operation to the buffer
    // Buffer'a bir yineleme islemi uygula
    void applyRedo(const Action& action, Buffer& buf);

    std::shared_ptr<UndoNode> root;       // Root of the undo tree / Geri alma agacinin koku
    std::shared_ptr<UndoNode> current;    // Current position in the tree / Agactaki mevcut konum
    int groupDepth_ = 0;                  // Nesting depth for beginGroup/endGroup / beginGroup/endGroup icin icleme derinligi
    int groupActionCount_ = 0;            // Actions recorded in current group / Mevcut grupta kaydedilen eylem sayisi
};
