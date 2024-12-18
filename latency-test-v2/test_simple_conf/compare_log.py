import json

def main():
    with open("db_ref.json") as f:
        db_ref = f.readlines()
    
    with open("db.json") as f:
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

if __name__ == "__main__":
    main()