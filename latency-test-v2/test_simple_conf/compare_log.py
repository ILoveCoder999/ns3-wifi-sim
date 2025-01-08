import json


class ErrorLogger:
    def __init__(self):
        self._error_cnt = 1

    def print_if_neq(self, arg1, arg2, msg):
        if arg1 != arg2:
            print("[ERROR {}] {}: {}, {}".format(self._error_cnt, msg, arg1, arg2))
            self._error_cnt+=1
    

def compare_v1():
    with open("db_ref.json") as f:
        db_ref = f.readlines()
    
    with open("db_data_rate.json") as f:
        db = f.readlines()

    db_ref = db_ref[1:-2]
    db = db[1:-2]

    for l_ref, l in zip(db_ref, db):
        l_ref = json.loads(l_ref.strip()[:-1])
        l = json.loads(l.strip()[:-1])
        for key in l_ref:
            if l[key] != l_ref[key]:
                print(key)
                print(l)
                print(l_ref)

def compare_v2():
    with open("db_ref_dropped.json") as f:
        db_ref = f.readlines()
    
    with open("db_new_dropped.json") as f:
        db = f.readlines()

    db_ref = db_ref[2:-2]
    db = db[2:-2]

    err_log = ErrorLogger()
    for l_ref, l in zip(db_ref, db):
        l_ref = json.loads(l_ref.strip()[:-1])
        l = json.loads(l.strip()[:-1])
        acked = l_ref["acked"]
        for key in l_ref:
            if key == "retransmissions":
                retrans = l_ref["retransmissions"]
                trans = l["transmissions"]
                #err_log.print_if_neq(len(retrans), l["retry"] if acked else len(trans), "wrong retry count")
                err_log.print_if_neq(len(retrans), (len(trans) - 1) if acked else len(trans), "wrong transmission number")
                new_retrans = [ {k:v for k,v in t.items() if k in r} for r, t in zip(retrans, trans)]
                err_log.print_if_neq(new_retrans, retrans, "transmissions != retransmissions")
                err_log.print_if_neq(trans[-1]["latency"], l["latency"], "different latency between last retry and ack/dropped")
                err_log.print_if_neq(l_ref["latency"], l["latency"], "different overall latency")                 
            else:
                err_log.print_if_neq(l[key], l_ref[key], "Fields {}".format(key))            

def compare_v3():
    with open("db.json") as f:
        db_ref = f.readlines()
    
    with open("db_new.json") as f:
        db = f.readlines()

    db_ref = db_ref[2:-2]
    db = db[2:-2]

    err_log = ErrorLogger()
    for l_ref, l in zip(db_ref, db):
        l_ref = json.loads(l_ref.strip()[:-1])
        l = json.loads(l.strip()[:-1])

        for t in l_ref["transmissions"]:
            t.pop("tx_time")
        for t in l["transmissions"]:
            t.pop("tx_time")

        for key in l_ref:
            err_log.print_if_neq(l[key], l_ref[key], "Fields {}".format(key))            




def main():
    # compare_v1()
    # compare_v2()
    compare_v3()

if __name__ == "__main__":
    main()