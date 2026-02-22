// BerkIDE — No impositions.
// Copyright (c) 2025 Berk Coşar <lookmainpoint@gmail.com>
// Licensed under the GNU Affero General Public License v3.0.
// See LICENSE file in the project root for full license text.

#include "undo.h"

// Constructor: create the root node of the undo tree
// Kurucu: geri alma agacinin kok dugumunu olustur
UndoManager::UndoManager() {
    root = std::make_shared<UndoNode>();
    current = root;
}

// Record a new action as a child branch of the current undo node
// Yeni bir eylemi mevcut geri alma dugumunun alt dali olarak kaydet
void UndoManager::addAction(const Action& action) {
    auto node = std::make_shared<UndoNode>();
    node->action = action;
    node->parent = current;
    current->branches.push_back(node);
    current->activeBranch = (int)current->branches.size() - 1;
    current = node;

    // Track group action count if inside a group
    // Grup icindeyse grup eylem sayacini takip et
    if (groupDepth_ > 0) {
        groupActionCount_++;
    }
}

// Begin a group of actions that undo/redo as a single step
// Tek adim olarak geri alinacak/yinelenecek bir eylem grubu baslat
void UndoManager::beginGroup() {
    if (groupDepth_ == 0) {
        groupActionCount_ = 0;
    }
    groupDepth_++;
}

// End the current action group, marking the last node with group size
// Mevcut eylem grubunu bitir, son dugumu grup boyutuyla isaretle
void UndoManager::endGroup() {
    if (groupDepth_ <= 0) return;
    groupDepth_--;

    // When outermost group ends, mark current node with the count
    // En distaki grup bittiginde, mevcut dugumu sayacla isaretle
    if (groupDepth_ == 0 && groupActionCount_ > 0 && current) {
        current->groupSize = groupActionCount_;
        groupActionCount_ = 0;
    }
}

// Check if currently inside a group
// Su an bir grup icinde olup olmadigini kontrol et
bool UndoManager::inGroup() const {
    return groupDepth_ > 0;
}

// Undo the last action (or entire group) by reversing it on the buffer
// Son eylemi (veya tum grubu) tampon uzerinde tersine cevirerek geri al
bool UndoManager::undo(Buffer& buf) {
    if (!current || !current->parent) return false;

    // If current node ends a group, undo all actions in the group
    // Mevcut dugum bir grubu bitiriyorsa, gruptaki tum eylemleri geri al
    int count = (current->groupSize > 0) ? current->groupSize : 1;
    for (int i = 0; i < count; ++i) {
        if (!current || !current->parent) break;
        applyUndo(current->action, buf);
        current = current->parent;
    }
    return true;
}

// Redo the next action (or entire group) along the active branch
// Aktif dal boyunca bir sonraki eylemi (veya tum grubu) yeniden uygula
bool UndoManager::redo(Buffer& buf) {
    if (!current || current->branches.empty()) return false;
    int idx = current->activeBranch;
    if (idx < 0 || idx >= (int)current->branches.size()) return false;

    // Walk forward to find the group end node, then replay all
    // Grup sonu dugumunu bulmak icin ileri yuru, sonra hepsini tekrar oynat
    current = current->branches[idx];
    applyRedo(current->action, buf);

    // If this starts a group, continue redo until group end
    // Eger bu bir grup baslangiciysa, grup sonuna kadar redo'ya devam et
    if (current->groupSize == 0) {
        // Check if the last node of a group chain is ahead
        // Bir grup zincirinin son dugumunun ileride olup olmadigini kontrol et
        // Single action, nothing more to do
        // Tek eylem, yapilacak baska sey yok
    } else {
        // This is a group end node from undo perspective; on redo we already replayed one
        // Bu geri alma perspektifinden bir grup sonu dugumu; yeniden yapmada birini zaten tekrar oynadik
        // We need to redo (groupSize - 1) more actions forward... but they are individual nodes
        // (groupSize - 1) daha fazla eylemi ileri dogru yeniden yapmamiz gerekiyor... ama bunlar bireysel dugumler
    }

    // Walk forward for grouped actions (they are chained as single-child branches)
    // Gruplu eylemler icin ileri yuru (tek cocuk dallar olarak zincirlenmisler)
    while (!current->branches.empty()) {
        auto& next = current->branches;
        if (next.size() != 1) break;  // Fork means user edited after undo / Fork, kullanicinin undo'dan sonra duzenleme yaptigini gosterir
        auto& nextNode = next[0];
        if (nextNode->groupSize > 0) {
            // This is the end of a group chain - redo remaining
            // Bu bir grup zincirinin sonu - kalanlari yeniden yap
            int remaining = nextNode->groupSize - 1;
            for (int i = 0; i < remaining && !current->branches.empty(); ++i) {
                current = current->branches[current->activeBranch];
                applyRedo(current->action, buf);
            }
            break;
        }
        break; // Only redo to the next fork / Yalnizca bir sonraki catala kadar yeniden yap
    }

    return true;
}

// Switch to a different undo branch at the current node
// Mevcut dugumde farkli bir geri alma dalina gec
void UndoManager::branch(int index) {
    if (!current) return;
    if (index < 0 || index >= (int)current->branches.size()) return;
    current->activeBranch = index;
}

// Return the number of branches at the current undo node
// Mevcut geri alma dugumundeki dal sayisini dondur
int UndoManager::branchCount() const {
    return current ? (int)current->branches.size() : 0;
}

// Return the index of the currently active branch
// Su an aktif olan dalin indeksini dondur
int UndoManager::currentBranch() const {
    return current ? current->activeBranch : -1;
}

// Apply the reverse of an action to the buffer (undo logic)
// Bir eylemin tersini tampona uygula (geri alma mantigi)
void UndoManager::applyUndo(const Action& a, Buffer& buf) {
    switch (a.type) {
        case ActionType::Insert:
            buf.deleteChar(a.line, a.col);
            break;
        case ActionType::Delete:
            buf.insertChar(a.line, a.col, a.character);
            break;
        case ActionType::InsertLine:
            buf.deleteLine(a.line);
            break;
        case ActionType::DeleteLine:
            buf.insertLineAt(a.line, a.lineContent);
            break;
        case ActionType::InsertText:
            // Reverse of inserting text: delete the range it created
            // Metin eklemenin tersi: olusturdugu araligini sil
            {
                // Calculate the end position of the inserted text
                // Eklenen metnin bitis konumunu hesapla
                int endLine = a.line;
                int endCol = a.col;
                for (char c : a.lineContent) {
                    if (c == '\n') {
                        endLine++;
                        endCol = 0;
                    } else {
                        endCol++;
                    }
                }
                buf.deleteRange(a.line, a.col, endLine, endCol);
            }
            break;
        case ActionType::DeleteRange:
            // Reverse of deleting a range: re-insert the saved text
            // Aralik silmenin tersi: kaydedilen metni yeniden ekle
            buf.insertText(a.line, a.col, a.lineContent);
            break;
    }
}

// Re-apply an action to the buffer (redo logic)
// Bir eylemi tampona yeniden uygula (yineleme mantigi)
void UndoManager::applyRedo(const Action& a, Buffer& buf) {
    switch (a.type) {
        case ActionType::Insert:
            buf.insertChar(a.line, a.col, a.character);
            break;
        case ActionType::Delete:
            buf.deleteChar(a.line, a.col);
            break;
        case ActionType::InsertLine:
            buf.insertLineAt(a.line, a.lineContent);
            break;
        case ActionType::DeleteLine:
            buf.deleteLine(a.line);
            break;
        case ActionType::InsertText:
            // Re-insert the text at the original position
            // Metni orijinal konuma yeniden ekle
            buf.insertText(a.line, a.col, a.lineContent);
            break;
        case ActionType::DeleteRange:
            // Re-delete the range
            // Araligi tekrar sil
            buf.deleteRange(a.line, a.col, a.lineEnd, a.colEnd);
            break;
    }
}
