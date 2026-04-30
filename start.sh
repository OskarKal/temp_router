#!/bin/bash

# Sprawdzenie, czy podano argument
if [ -z "$1" ]; then
    echo "Błąd: Nie podano argumentu."
    echo "Użycie: ./start.sh <numer_maszyny>"
    echo "Dostępne numery: 0, 1, 2, 3"
    exit 1
fi

MACHINE=$1
echo "Konfigurowanie maszyny Virbian$MACHINE..."

case $MACHINE in
    0)
        # Maszyna 0: Sieci A i B
        sudo ip addr add 10.0.1.1/26 dev enp0s3
        sudo ip link set dev enp0s3 up
        sudo ip addr add 10.0.2.1/27 dev enp0s8
        sudo ip link set dev enp0s8 up

        cat << 'EOF' > router.conf
2
10.0.1.1/26 distance 2
10.0.2.1/27 distance 3
EOF
        ;;
    1)
        # Maszyna 1: Sieci A i C
        sudo ip addr add 10.0.1.2/26 dev enp0s3
        sudo ip link set dev enp0s3 up
        sudo ip addr add 10.0.3.1/28 dev enp0s8
        sudo ip link set dev enp0s8 up

        cat << 'EOF' > router.conf
2
10.0.1.2/26 distance 2
10.0.3.1/28 distance 1
EOF
        ;;
    2)
        # Maszyna 2: Sieci C i D
        sudo ip addr add 10.0.3.2/28 dev enp0s3
        sudo ip link set dev enp0s3 up
        sudo ip addr add 10.0.4.1/29 dev enp0s8
        sudo ip link set dev enp0s8 up

        cat << 'EOF' > router.conf
2
10.0.3.2/28 distance 1
10.0.4.1/29 distance 4
EOF
        ;;
    3)
        # Maszyna 3: Sieci D i B
        sudo ip addr add 10.0.4.2/29 dev enp0s3
        sudo ip link set dev enp0s3 up
        sudo ip addr add 10.0.2.2/27 dev enp0s8
        sudo ip link set dev enp0s8 up

        cat << 'EOF' > router.conf
2
10.0.4.2/29 distance 4
10.0.2.2/27 distance 3
EOF
        ;;
    *)
        echo "Błąd: Nieznany numer maszyny. Wybierz 0, 1, 2 lub 3."
        exit 1
        ;;
esac

echo "Generowanie router.conf zakończone."
echo "Uruchamianie ./router..."
echo "----------------------------------------"

# Uruchomienie programu
./router < router.conf
