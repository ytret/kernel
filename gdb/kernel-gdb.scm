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
  (format #t "id ~d\n" (value->integer (value-field task "id"))))

(define (cmd/y self args from-tty)
  (execute "help y" #:from-tty #t))

(define (cmd/y/tasks self args from-tty)
  (execute "help y tasks" #:from-tty #t))

(define (cmd/y/tasks/all self args from-tty)
  (let ((tasks (task/listify-all-tasks)))
    (if (equal? 0 (length tasks))
      (display "No tasks.\n")
      (map task/print-task tasks))))

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
      (map task/print-task tasks))))

(define (cmd/y/tasks/sleeping self args from-tty)
  (let ((tasks (task/listify-sleeping-tasks)))
    (if (equal? 0 (length tasks))
      (display "No sleeping tasks.\n")
      (map task/print-task tasks))))

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
