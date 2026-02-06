[Задание](task.pdf)

## Модуль

Модуль ядра для Linux 6.12 записывает строки в файл в директории `/var/tmp/test_module` по таймеру.
Через sysfs можно изменять/получать интервал в секундах и имя файла.

Изменение интервала отменяет следующую запись. И если интервал не равен 0, перезапускает таймер.

Сборка:

```sh
make KDIR=<kernel path> -C kernel_module # Если опустить KDIR, будет использовано текущее
```

Генерация `compile_commands.json` (нужен [bear](https://github.com/rizsotto/Bear)):

```sh
make -C kernel_module compile_commands.json
```

Для добавления и удаления из текущего ядра:

```sh
make -C kernel_module load
make -C kernel_module unload
```

## userspace программа

Просто пишет и читает атрибуты из `/sys/kernel/test_module/`.

Генерация `compile_commands.json` (нужен [bear](https://github.com/rizsotto/Bear)):

```sh
make -C userspace compile_commands.json
```

Сборка:

```sh
make -C userspace
```

help:

```sh
userspace/build/test_module_ctl -h
```
