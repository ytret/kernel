(use-modules (gdb))
(use-modules (ice-9 format))

(define type/task (lookup-type "task_t"))
(define type/list (lookup-type "list_t"))
(define type/list-node (lookup-type "list_node_t"))
(define null-ptr (make-value 0))

(define (get-field-offset struct-type field-name)
  (let* ((zero-ptr (value-cast (make-value 0) (type-pointer struct-type)))
         (field (value-field (value-dereference zero-ptr) field-name)))
    (value->integer (value-address field))))

(define (value-is-pointer? value)
  (eq? TYPE_CODE_PTR (type-code (value-type value))))

(define (list/node->struct node struct-type field-name)
  (value-cast
    (make-value (- (value->integer (value-address node))
                   (get-field-offset struct-type field-name)))
    (type-pointer struct-type)))

(define (list/get-next-node node)
  (let ((next-ptr (value-field node "p_next")))
    (if (equal? next-ptr null-ptr) #f (value-dereference next-ptr))))

(define (list/listify-nodes node)
  (if node
      (let ((next-node (list/get-next-node node)))
        (append (list node)
                (if next-node (list/listify-nodes next-node) '())))
      '()))

(define (list/length list-val)
  (let ((first-node-ptr (value-field list-val "p_first_node")))
    (if (equal? first-node-ptr null-ptr)
      0
      (length (list/listify-nodes (value-dereference first-node-ptr))))))

(define* (task/listify-tasks-list
           symbol-name
           #:optional (node-name-in-struct "list_node"))
  (map
    (lambda (node) (list/node->struct node type/task node-name-in-struct))
    (list/listify-nodes
      (let*
        ((tasks-list (symbol-value (car (lookup-symbol symbol-name))))
         (first-node-ptr (value-field tasks-list "p_first_node")))
        (if (equal? first-node-ptr null-ptr)
          #f
          (value-dereference first-node-ptr))))))

(define (task/listify-all-tasks)
  (task/listify-tasks-list "g_all_tasks" "all_tasks_list_node"))

(define (task/listify-runnable-tasks)
  (task/listify-tasks-list "g_runnable_tasks"))

(define (task/listify-sleeping-tasks)
  (task/listify-tasks-list "g_sleeping_tasks"))

(define (task/print-task task)
  (let* ((kernel-stack (value-field task "kernel_stack"))
         (esp0-max (value->integer (value-field kernel-stack "p_top_max")))
         (esp0-top (value->integer (value-field kernel-stack "p_top")))
         (esp0-bot (value->integer (value-field kernel-stack "p_bottom"))))
    (format #t "id ~3d  esp0: (max 0x~8,'0x, top 0x~8,'0x, bot 0x~8,'0x, used ~d)\n"
            (value->integer (value-field task "id"))
            esp0-max
            esp0-top
            esp0-bot
            (- esp0-max esp0-top))))

(define (heap/listify-tags)
  (list/listify-nodes
    (let ((tag-ptr (symbol-value (car (lookup-symbol "gp_heap_start")))))
      (if (equal? tag-ptr null-ptr)
        #f
        (value-dereference tag-ptr)))))

(define (heap/print-tag tag)
  (let* ((addr (if (value-is-pointer? tag)
                 (value->integer tag)
                 (value->integer (value-address tag))))
         (used? (value->bool (value-field tag "b_used")))
         (size (value->integer (value-field tag "size"))))
    (format #t "at 0x~8,'0x, ~a, ~d bytes\n"
            addr (if used? "used" "free") size)))

(define (cmd/y self args from-tty)
  (execute "help y" #:from-tty #t))

(define (cmd/y/tasks self args from-tty)
  (execute "help y tasks" #:from-tty #t))

(define (cmd/y/tasks/all self args from-tty)
  (let ((tasks (task/listify-all-tasks)))
    (if (equal? 0 (length tasks))
      (display "No tasks.\n")
      (for-each task/print-task tasks))))

(define (cmd/y/tasks/running self args from-tty)
  (let ((running-task
          (let ((ptr (symbol-value (car (lookup-symbol "gp_running_task")))))
            (if (equal? ptr null-ptr) #f (value-dereference ptr)))))
    (if running-task
      (task/print-task running-task)
      (display "No running task.\n"))))

(define (cmd/y/tasks/runnable self args from-tty)
  (let ((tasks (task/listify-runnable-tasks)))
    (if (equal? 0 (length tasks))
      (display "No runnable tasks.\n")
      (for-each task/print-task tasks))))

(define (cmd/y/tasks/sleeping self args from-tty)
  (let ((tasks (task/listify-sleeping-tasks)))
    (if (equal? 0 (length tasks))
      (display "No sleeping tasks.\n")
      (for-each task/print-task tasks))))

(define (cmd/y/list self args from-tty)
  (execute "help y list" #:from-tty #t))

(define (cmd/y/list/length self args from-tty)
  (let* ((var-name args)
         (symbol-f (lookup-symbol var-name)))
    (if symbol-f
      (let ((symbol (car symbol-f)))
        (if (equal? type/list (symbol-type symbol))
          (format #t "~d\n" (list/length (symbol-value symbol)))
          (format #t "Symbol ~s does not have type ~s.\n"
                  (symbol-name symbol) (type-name type/list))))
      (format #t "Could not find symbol ~s.\n" var-name))))

(define (cmd/y/heap self args from-tty)
  (execute "help y heap" #:from-tty #t))

(define (cmd/y/heap/dump self args from-tty)
  (let ((i 0))
    (for-each
      (lambda (tag)
        (format #t "~3d. " i)
        (heap/print-tag tag)
        (set! i (+ i 1)))
      (heap/listify-tags))))

(define (cmd/y/heap/stat self args from-tty)
  (let ((used-bytes 0)
        (free-bytes 0)
        (used-tags 0)
        (free-tags 0))
    (for-each
      (lambda (tag)
        (let ((used? (value->bool (value-field tag "b_used")))
              (size (value->integer (value-field tag "size"))))
          (if used?
            (begin
              (set! used-bytes (+ used-bytes size))
              (set! used-tags (+ used-tags 1)))
            (begin
              (set! free-bytes (+ free-bytes size))
              (set! free-tags (+ free-tags 1))))))
      (heap/listify-tags))
    (let* ((total-bytes (+ used-bytes free-bytes))
           (total-tags (+ used-tags free-tags))
           (max-width
             (lambda (num-list)
               (apply max
                      (map string-length
                           (map (lambda (n) (format #f "~:d" n)) num-list)))))
           (bytes-width (max-width (list total-bytes used-bytes free-bytes)))
           (tags-width (max-width (list total-tags used-tags free-tags))))
      (format #t "Total bytes: ~v:d\n" bytes-width total-bytes)
      (format #t " Used bytes: ~v:d (~,1,2f %)\n"
              bytes-width used-bytes (/ used-bytes total-bytes))
      (format #t " Free bytes: ~v:d (~,1,2f %)\n"
              bytes-width free-bytes (/ free-bytes total-bytes))
      (format #t "Total tags: ~v:d\n" tags-width total-tags)
      (format #t " Used tags: ~v:d (~,1,2f %)\n"
              tags-width used-tags (/ used-tags total-tags))
      (format #t " Free tags: ~v:d (~,1,2f %)\n"
              tags-width free-tags (/ free-tags total-tags)))))

(register-command! (make-command "y"
                                 #:invoke cmd/y
                                 #:prefix? #t
                                 #:doc "Custom kernel-related commands."))

(register-command! (make-command "y tasks"
                                 #:invoke cmd/y/tasks
                                 #:prefix? #t
                                 #:doc "Task enumeration commands."))
(register-command!
  (make-command "y tasks all"
                #:invoke cmd/y/tasks/all
                #:command-class COMMAND_DATA
                #:doc "Print all tasks regardless of their state."))
(register-command!
  (make-command "y tasks running"
                #:invoke cmd/y/tasks/running
                #:command-class COMMAND_DATA
                #:doc "Print the running task."))
(register-command!
  (make-command "y tasks runnable"
                #:invoke cmd/y/tasks/runnable
                #:command-class COMMAND_DATA
                #:doc "Print runnable tasks."))
(register-command!
  (make-command "y tasks sleeping"
                #:invoke cmd/y/tasks/sleeping
                #:command-class COMMAND_DATA
                #:doc "Print sleeping tasks."))

(register-command! (make-command "y list"
                                 #:invoke cmd/y/list
                                 #:prefix? #t
                                 #:doc "List enumeration commands."))
(register-command!
  (make-command "y list length"
                #:invoke cmd/y/list/length
                #:command-class COMMAND_DATA
                #:doc "Print the number of nodes in a list."))

(register-command!
  (make-command "y heap"
                #:invoke cmd/y/heap
                #:prefix? #t
                #:doc "Heap statistics commands."))
(register-command!
  (make-command "y heap dump"
                #:invoke cmd/y/heap/dump
                #:command-class COMMAND_DATA
                #:doc "Print the heap tags."))
(register-command!
  (make-command "y heap stat"
                #:invoke cmd/y/heap/stat
                #:command-class COMMAND_DATA
                #:doc "Print the heap usage statistics."))
