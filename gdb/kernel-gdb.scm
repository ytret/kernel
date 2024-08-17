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

(define (task/print-task task)
  (format #t "id ~d\n" (value->integer (value-field task "id"))))

(define (cmd/print-tasks self args from-tty)
  (let ((tasks (task/listify-runnable-tasks)))
    (if (equal? 0 (length tasks))
      (display "No tasks.\n")
      (map task/print-task tasks))))

(register-command! (make-command "y" #:prefix? #t))
(register-command!
  (make-command "y tasks"
                #:invoke cmd/print-tasks
                #:command-class COMMAND_DATA
                #:doc "Print tasks."))
