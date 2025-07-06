(define-module (task)
  #:use-module (gdb)
  #:use-module ((linked-list) #:prefix ll/)
  #:use-module (util))

(define* (listify-tasks-list
           symbol-name
           #:optional (node-name-in-struct "list_node"))
  (map
    (lambda (node) (ll/node->struct node type/task node-name-in-struct))
    (ll/listify-nodes
      (let*
        ((tasks-list (symbol-value (car (lookup-symbol symbol-name))))
         (first-node-ptr (value-field tasks-list "p_first_node")))
        (if (equal? first-node-ptr null-ptr)
          #f
          (value-dereference first-node-ptr))))))
(export listify-tasks-list)

(define-public (listify-all-tasks)
  (listify-tasks-list "g_taskmgr_all_tasks" "all_tasks_list_node"))

(define-public (listify-runnable-tasks)
  (listify-tasks-list "g_runnable_tasks"))

(define-public (listify-sleeping-tasks)
  (listify-tasks-list "g_sleeping_tasks"))

(define-public (print-task task)
  (let* ((taskmgr (value-field task "taskmgr"))
         (kernel-stack (value-field task "kernel_stack"))
         (esp0-max (value->integer (value-field kernel-stack "p_top_max")))
         (esp0-top (value->integer (value-field kernel-stack "p_top")))
         (esp0-bot (value->integer (value-field kernel-stack "p_bottom"))))
    (format #t "id ~3d  cpu ~2d  esp0: (max 0x~8,'0x, top 0x~8,'0x, bot 0x~8,'0x, used ~4d) ~32s ~a ~a ~a M:~d\n"
            (value->integer (value-field task "id"))
            (value->integer (value-field taskmgr "proc_num"))
            esp0-max
            esp0-top
            esp0-bot
            (- esp0-max esp0-top)
            (value->string (value-field task "name") #:encoding "ascii")
            (if (equal? 0 (value->integer (value-field task "is_blocked")))
              "b" "B")
            (if (equal? 0 (value->integer (value-field task "is_terminating")))
              "t" "T")
            (if (equal? 0 (value->integer (value-field task "is_sleeping")))
              "s" "S")
            (value->integer (value-field task "num_owned_mutexes")))))
