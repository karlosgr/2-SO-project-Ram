#include <stdio.h>
#include "bnb_manager.h"
#include "structs.h"
#include "memory.h"

static process_t actual_proc;

static List bnb;

static lf_list frees;

// Esta función se llama cuando se inicializa un caso de prueba
void m_bnb_init(int argc, char **argv)
{
    // debug_bnb = fopen("./Debug/Debug.txt", "w");

    bnb = Init();
    frees = Init_LF(m_size());
}

// Reserva un espacio en el heap de tamaño 'size' y establece un puntero al
// inicio del espacio reservado.
int m_bnb_malloc(size_t size, ptr_t *out)
{
    bandb *proc = Find(actual_proc, &bnb);
    out->size = size;
    out->addr = proc->heap - proc->base;
    proc->heap += size;
    return proc->heap >= proc->stack ? 1 : 0;
}

// Libera un espacio de memoria dado un puntero.
int m_bnb_free(ptr_t ptr)
{
    bandb *proc = Find(actual_proc, &bnb);
    size_t from_addr = Search_addr(ptr.addr, &proc->mask_addr);
    size_t to_addr = from_addr + ptr.size;
    for (size_t i = to_addr; i < proc->heap; i++)
    {
        m_write(from_addr, m_read(to_addr));
        from_addr++;
    }
    for (size_t i = 0; i < proc->mask_addr.length; i++)
    {
        if (proc->mask_addr.start[i].addr_v >= ptr.addr)
        {
            proc->mask_addr.start[i].addr_r -= ptr.size;
        }
    }
    proc->heap -= ptr.size;
    return 0;
}

// Agrega un elemento al stack
int m_bnb_push(byte val, ptr_t *out)
{

    bandb *proc = Find(actual_proc, &bnb);
    proc->stack--;
    m_write(proc->stack, val);
    out->addr = proc->stack - proc->base;
    out->size = 1;
    return proc->stack == proc->heap ? 1 : 0;
}

// Quita un elemento del stack
int m_bnb_pop(byte *out)
{
    bandb *proc = Find(actual_proc, &bnb);
    *out = m_read(proc->stack);
    proc->stack++;
    return 0;
}

// Carga el valor en una dirección determinada
int m_bnb_load(addr_t addr, byte *out)
{
    bandb *proc = Find(actual_proc, &bnb);
    size_t address = Search_addr(addr, &proc->mask_addr);
    if (address == 0)
        *out = m_read(addr);
    else
        *out = m_read(address);

    return 0;
}

// Almacena un valor en una dirección determinada
int m_bnb_store(addr_t addr, byte val)
{
    bandb *proc = Find(actual_proc, &bnb);
    Add_Mask(addr, proc->heap, &proc->mask_addr);
    m_write(proc->heap, val);
    proc->heap++;
    return proc->heap == proc->stack ? 1 : 0;
}

// Notifica un cambio de contexto al proceso 'next_pid'
void m_bnb_on_ctx_switch(process_t process)
{
    if (!Exist(process, &bnb)) // verifico primero que el proceso nuevo ya tenga un espacio en memoria reservado
    {
        lf_element filled = Fill_Space(512, &frees);
        mask newmask = Init_Mask();                                                                                                      // si no es asi le reservo 64 bytes de memoria
        bandb new = {process, filled.start, filled.size, filled.start + process.program->size, newmask, filled.start + filled.size - 1}; // creo un objeto bandb donde alammceno el proceso con su información de su memoria reservada
        Push(new, &bnb);                                                                                                                 // lo agrego a la lista de bandb
        set_curr_owner(process.pid);
        m_set_owner(filled.start, filled.start + filled.size - 1); // pongo al proceso como el owner de ese espacio de memoria
    }
    actual_proc = process;
}

// Notifica que un proceso ya terminó su ejecución
void m_bnb_on_end_process(process_t process)
{
    bandb removed = *Find(process, &bnb);
    Remove(removed, &bnb);
    Free_Space(removed.base, removed.bounds, &frees);
    m_unset_owner(removed.base, removed.base + removed.bounds - 1);
}
