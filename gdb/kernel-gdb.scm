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

(define (task/get-running-task)
  (let ((ptr (car (lookup-symbol "gp_running_task"))))
    (if ptr (value-dereference (symbol-value ptr)) #f)))

(define (task/get-runnable-tasks)
  (symbol-value (car (lookup-symbol "g_runnable_tasks"))))

(define (task/get-first-runnable-task-node)
  (let ((first-node-ptr
          (value-field (task/get-runnable-tasks) "p_first_node")))
    (if (equal? first-node-ptr (make-value 0))
      #f
      (value-dereference first-node-ptr))))

(define (task/listify-runnable-task-nodes)
  (list/listify-nodes (task/get-first-runnable-task-node)))

(define (task/listify-runnable-tasks)
  (map
    (lambda (node) (list/node->struct node type/task "list_node"))
    (task/listify-runnable-task-nodes)))

(define (task/listify-all-tasks)
  (map
    (lambda (node) (list/node->struct node type/task "all_tasks_list_node"))
    (list/listify-nodes
      (let*
        ((all-tasks-list (symbol-value (car (lookup-symbol "g_all_tasks"))))
         (first-node-ptr (value-field all-tasks-list "p_first_node")))
        (if (equal? first-node-ptr null-ptr)
          #f
          (value-dereference first-node-ptr))))))

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

(register-command! (make-command "y"
                                 #:invoke cmd/y
                                 #:prefix? #t
                                 #:doc "Custom kernel-related commands"))
(register-command! (make-command "y tasks"
                                 #:invoke cmd/y/tasks
                                 #:prefix? #t
                                 #:doc "Task enumeration commands"))
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
