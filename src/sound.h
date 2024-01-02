/*
  音声データは配布していませんので変換してください。

  作成方法
    python file2h.py input.wav output.h
    変換したらコピペして変数名を変える
*/

// のだー
const unsigned char sound000[] PROGMEM = {
  // コピペする
};

// くすぐったいのだ
const unsigned char sound001[] PROGMEM = {
  // コピペする
};

// 各サウンドデータを配列にしたものをカンマで区切って追加していく
const unsigned char* soundFlashData[] PROGMEM = { 
  sound000, // のだー
  sound001 // くすぐったいのだ
};
const size_t soundFlashSize[] PROGMEM = {
  sizeof(sound000),
  sizeof(sound001)
};
