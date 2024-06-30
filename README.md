![obraz](https://github.com/WojtekMatr/Programowanie-rownolegle-wieloprocesowosc/assets/127395210/42aa049e-f0cb-4cae-9765-4ddc877ad7fb)
Program w C który ma 1 proces matke i 3 podprocesy, 1 proces odczytuje tekst i wysyła do 2 procesu, 2 liczy liczbę liter i wysyła do 3 procesu 3 wyświetla liczbę liter i wraca do 1 z następną linijką tekstu. Program ma 3 sygnały, możemy wysyłać do procesu, a cały program:
1-zatrzyma się 
2-wznowi się
3 zatrzyma się
Do stworzenia programu wykorzystałem semafory, kolejkę i pipe(do komunikowania się między procesami)
[![Obejrzyj aplikacje]](https://www.youtube.com/watch?v=TF5eE2BPV7c)
