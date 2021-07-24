# ESP_sOnOff
DIY ESP8266 - Node MCU 3 channel sonoff switch with MQTT and HTTP support
=========================================================================
Δημοσιεύω την κατασκευή ενός διακόπτη τύπου SONOFF με το ESP8266. Το βασικό πλεονέκτημα έναντι των διακοπτών SONOFF του εμπορίου, είναι ότι μπορεί να λειτουργήσει και σε διάταξη Aller Retour ενώ στον τοίχο παραμένουν οι μηχανικοί διακόπτες για χειροκίνητη λειτουργία. 
Αν στην υφιστάμενη εγκατάσταση υπήρχε απλός διακόπτης, τότε αυτός πρέπει να αντικατασταθεί με Aller Retour ώστε να γίνεται ο χειρισμός είτε χειροκίνητα είτε από το ηλεκτρονικό κύκλωμα. Αν υπήρχε ήδη διάταξη Aller Retour τότε ο ένας (κοντά στο κύκλωμα) πρέπει να αντικατασταθεί από ενδιάμεσο.
Το κύκλωμα καταλαβαίνει αν υπάρχει ροή ρεύματος και όταν το φως ανάψει χειροκίνητα, ο χρήστης βλέπει την κατάσταση 'Αναμμένο'.
Ο χειρισμός γίνεται από HTTP εφόσον στο ESP8266 λειτουργεί Web Server ή μέσα από MQTT 
<img src="https://github.com/stav98/ESP_sOnOff/blob/main/images/prototype1.jpg" width="600"><br>
<img src="https://github.com/stav98/ESP_sOnOff/blob/main/images/screenshot1.png" width="400">&nbsp;&nbsp;<img src="https://github.com/stav98/ESP_sOnOff/blob/main/images/screenshot2.png" width="400"><br>
<img src="https://github.com/stav98/ESP_sOnOff/blob/main/images/screenshot3.png" width="400">&nbsp;&nbsp;<img src="https://github.com/stav98/ESP_sOnOff/blob/main/images/screenshot4.png" width="400"><br>
<img src="https://github.com/stav98/ESP_sOnOff/blob/main/images/screenshot5.png" width="400"><br>
