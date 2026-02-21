#pragma once
#include <string>
#include <vector>

// Line-based piece table for efficient text storage.
// Verimli metin depolama icin satir tabanli piece table.
// Stores original lines immutably; edits go to an append-only add buffer.
// Orijinal satirlari degistirmez saklar; duzenlemeler yalnizca ekleme arabellegine gider.
class PieceTable {
public:
    // Piece source: either the original loaded text or the add buffer
    // Parca kaynagi: ya orijinal yuklenen metin ya da ekleme arabellegi
    enum class Source { Original, Add };

    // A piece descriptor: points to a contiguous range of lines in a source buffer
    // Parca tanimlayicisi: bir kaynak arabellekteki bitisik satir araligina isaret eder
    struct Piece {
        Source source;
        int start;   // Starting line index in source buffer / Kaynak arabellekteki baslangic satir indeksi
        int count;   // Number of lines in this piece / Bu parcadaki satir sayisi
    };

    PieceTable();

    // Get a read-only copy of a line
    // Bir satirin salt okunur kopyasini al
    std::string getLine(int line) const;

    // Get a mutable reference (copy-on-write: original lines are copied to add buffer)
    // Degistirilebilir referans al (yazimda kopyala: orijinal satirlar ekleme arabellegine kopyalanir)
    std::string& getLineRef(int line);

    // Total number of logical lines
    // Toplam mantiksal satir sayisi
    int lineCount() const;

    // Number of characters in a specific line
    // Belirli bir satirdaki karakter sayisi
    int columnCount(int line) const;

    // Insert a new line at a specific index
    // Belirli bir indekse yeni satir ekleme
    void insertLineAt(int index, const std::string& line);

    // Append a line at the end
    // Sona satir ekleme
    void appendLine(const std::string& line);

    // Delete the line at a given index
    // Verilen indeksteki satiri silme
    void deleteLine(int index);

    // Replace the content of a line (COW for original lines)
    // Bir satirin icerigini degistirme (orijinal satirlar icin COW)
    void setLine(int index, const std::string& content);

    // Check if (line, col) is a valid position
    // (satir, sutun) gecerli bir konum mu kontrol et
    bool isValidPos(int line, int col) const;

    // Load lines in bulk (for file open), replaces all content
    // Toplu satir yukleme (dosya acma icin), tum icerigi degistirir
    void loadLines(std::vector<std::string>&& lines);

    // Load lines from a const vector (copy)
    // Const vektorunden satir yukleme (kopyalama)
    void loadLines(const std::vector<std::string>& lines);

    // Clear all content, reset to single empty line
    // Tum icerigi temizle, tek bos satira sifirla
    void clear();

    // Get all lines as a vector (materialized view)
    // Tum satirlari vektor olarak al (somutlastirilmis gorunum)
    std::vector<std::string> allLines() const;

    // Get the piece count (for diagnostics)
    // Parca sayisini al (tanilar icin)
    int pieceCount() const;

    // Compact pieces: merge adjacent pieces from the same source
    // Parcalari sikistir: ayni kaynaktan bitisik parcalari birlestir
    void compact();

private:
    std::vector<std::string> original_;  // Immutable original lines / Degistirilemez orijinal satirlar
    std::vector<std::string> add_;       // Append-only add buffer / Yalnizca ekleme arabellegi

    std::vector<Piece> pieces_;          // Piece descriptors / Parca tanimlayicilari
    int lineCount_ = 0;                  // Cached total line count / Onbelleklenmis toplam satir sayisi

    // Locate which piece and offset contains a logical line index
    // Mantiksal satir indeksini iceren parca ve ofset'i bul
    struct PiecePos {
        int pieceIdx;
        int offset;    // Offset within the piece / Parca icindeki ofset
    };
    PiecePos findLine(int line) const;

    // Split a piece at the given offset (offset becomes start of new piece)
    // Verilen ofset'te parcayi bol (ofset yeni parcanin baslangici olur)
    void splitPiece(int pieceIdx, int offset);

    // Recalculate cached line count
    // Onbelleklenmis satir sayisini yeniden hesapla
    void recalcLineCount();

    // Get line reference from a piece position
    // Parca konumundan satir referansi al
    const std::string& lineAtConst(const Piece& piece, int offset) const;
};
