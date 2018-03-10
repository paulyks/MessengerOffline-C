# MessengerOffline - C
<b>Introducere</b>         
     In urmatoarele randuri, este prezentata aplicatia client-server. Utilizatorii aplicatiei
vor intalni un meniu familiar ce le permite comunicarea privata intre doi
utilizatori sau mai multi(lobbyuri), fie ei online, fie offline. In cazul in care utilizatorul
care primeste mesaje este offline, acesta va primi o notificare la logare cum
ca are mesage noi. Aplicatia va salva toate conversatiile, utilizatorii putand sa
isi verifice istoricul conversatiilor. Deasemene aplicatia mai faciliteaza utilizatorii
printr-o lista de prieteni si o gama de setari ce tin de interfata sau de partea audio.
Administratorii nu au fost neglijati, ei avand la dispozitie o comanda prin care pot
intra intr-un meniu secret, nestiut de utilzatorii normali.

<b>Tehnologii utilizate</b>            
     Am ales sa folosesc protocolul orientat pe conexiune TCP/IP pentru a avea o
conexiune permanenta intre utilizatori si server. Cum datele trimise de pe client
la server si invers sunt livrate fara erori, am fost convins de alegerea facuta.
Totodata, m-am folosit si de proprietatea multplexarii, avand mai multe threaduri
ce ruleaza pe acelasi host.

<b>Detalii de implementare</b>                   
     In momentul deschiderii serverului, el va astepta in permanenta conectarea cu
clientii folosind primitiva select. Cand un client nou se conecteaza, el este introdus
intr-un set ce va fi ascultat in permanenta pentru receptionarea de mesaje. In momentul
in care un serverul primeste un mesaj de la client, descriptorul respectiv
este eliminat din set. In momentul acesta este creat un thread, iar comunicarea
dintre respectivul client si server se va face doar pe threadul nou creat. Taskurile
cerute de client(ex: afiseaza userii online, deschide o conversatie, adauga user
in lista de prieteni, etc) se fac pe baza unui cod numeric. In threadul ce se va
ocupa de client, se va identifica portiunea echivalenta cu codul numeric primit si
va incepe o serie de schimburi de informatii dintre client si server. La sfarsitul
procesarii datelor cerute, clientul este adaugat inapoi in setul de clienti ce sunt ascultati
in permanenta de server pentru receptionarea urmatorului cod numeric, iar
threadul este inchis. Serverul nu va astepta niciodata inputul unui client, ceea ce
faciliteaza stabilitatea acestuia deoarece erorile intalnire la primitivele read/write
vor fi diminuate.

     La fel ca si pe server, protocolul clientului este structurat tot pe coduri numerice.
Pentru ca clientul sa poata primi notificari in timp real, aplicatia se foloseste
de primitiva select si in acest end-point. Clientul avanseaza in meniul sau
tot cu un cod numeric ce este primit de server(acolo unde este nevoie) in functie
de cerintele cerute. Cand clientul primeste un cod numeritc, se creeaza un nou
thread, iar un flag ce blocheaza crearea altui thread este activat. In threadul nou
creat, se cauta portiunea de cod echivalenta cu codul numeric si incepe o serie de
schimburi de informatii intre client si server. Pentru ca primitiva select inca asculta
informatiile ce vin pe socket, un flag ce blocheaza citirea in afara threadului
este activat, acolo unde este cazul. In cazul in care pe threadul nou creat nu se
fac operatiuni de citire, flagul ce blocheaza citirea nu este activat.

     Utilizatorul aplicatiei dispune de multiple facilitati in cadrul aplicatiei, cum ar
fi:
- inregistrarea in baza de date a unui cont nou
- logarea pe server cu contul creat
- adaugarea/stergerea din lista de prieteni a unui utilizator din baza de date
- vizualizarea tuturor utilizatorilor online in retea
- vizualizarea tuturor utilizatorilor din retea
- cautarea utilizatorilor in retea dupa nume sau fragmente din nume
- verificarea unui inbox in care sunt afisati utilizatorii de la care are mesaje necitite
- deschiderea unei conversatii cu o persoana(mesajele necitite vor fi evidentiate)
- posibilitatea de a conversa in timp real cu utilizatorii aplicatiei
- notificari audio atunci cand se primeste un mesaj nou sau cand utilizatorul intra
in meniul general
- posibilitatea aderarii unui chatroom public sau privat(cu parola)
- posibilitatea crearii unui chatroom public sau privat(cu parola)
- posibilitatea listarii tuturor chatroom-urilor disponibile
- posibilitatea cautarii unui chatroom dupa nume sau fragmente din nume
- setari privind activarea/dezactivarea sunetului de notificare sau a melodiei de la
meniul general
- setari privind culoarea textului din cadrul aplicatiei
             
     In meniul general a fost ascunsa o comanda ce o cunosc doar administratorii
aplicatiei. Aceasta comanda le permite navigarea intr-un meniu cu totul diferit ce
ofera urmatoarele facilitati:
- stergerea din baza de date a unui utilizator
- stergerea conversatiei din baza de date dintre doi utilizatori
