; boot.asm --- BIOSが直接実行する512バイトのブートセクタ
BITS 16                 ; CPUは起動直後、16bitリアルモードで動いている
ORG 0x7C00              ; BIOSはこのコードをメモリの0x7C00番地に置く、という約束

start:
    mov si, msg         ; SIレジスタに文字列の先頭アドレスを入れる
.loop:
    lodsb               ; SIの指す1バイトをALに読み、SIを1進める
    or al, al           ; ALが0(文字列の終端)なら
    jz .halt            ; 終了処理へ
    mov ah, 0x0E        ; BIOSの機能番号: 「1文字表示」(テレタイプ出力)
    int 0x10            ; BIOSを呼び出す(画面系サービスは割り込み0x10番)
    jmp .loop

.halt:
    hlt                 ; CPUを休止させる
    jmp .halt           ; 念のため無限ループ(戻る場所がないので止め続ける)

msg db "Hello from boot sector! (512 bytes, no OS)", 0

times 510-($-$$) db 0   ; ここまでを0埋めして、ちょうど510バイトにする
dw 0xAA55               ; 末尾2バイトの署名。BIOSはこれを見て「起動可能」と判断
